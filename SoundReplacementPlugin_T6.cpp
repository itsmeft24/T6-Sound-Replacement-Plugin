
#define DeclareFunction(ret_type, call_conv, name, offset, ...) auto name = (ret_type(call_conv *)(##__VA_ARGS__))(offset)

#include <iostream>
#include <filesystem>
#include <fstream>
#include <unordered_map>
#include <Windows.h>

#include "HookFunction.h"
#include "Resource.h"

DeclareFunction(bool, __cdecl, Stream_LoadFileSynchronously_Original, 0x008900A0);
DeclareFunction(bool, __cdecl, Stream_ReadInternal, 0x004C9F50, HANDLE, uint64_t, LPVOID, DWORD, DWORD, uint64_t*);
DeclareFunction(void, __cdecl, Stream_SeekInternal, 0x004473F0, HANDLE, uint64_t);
DeclareFunction(bool, __cdecl, SND_AssetBankValidateEntry_Original, 0x0053B6E0, uint32_t, SndAssetBankHeader*, SndAssetBankEntry*);
DeclareFunction(void, __cdecl, SD_StreamReadCallback, 0x009145C0, int, stream_status, unsigned int, void*);
DeclareFunction(void, __cdecl, SND_LoadAssetCallback, 0x00913B80, int, stream_status, unsigned int, void*);

// A struct representing a modded sound.
struct ModdedSound {
	std::filesystem::path absoluteFilePath;
	std::string relativeAssetPath;
	uint64_t baseOffset;
};

// Global variable for all our patched modfiles.
std::unordered_map<uint32_t, ModdedSound> CollectedFiles;

// Simple and modern SND_HashName implementation. *chef's kiss*
unsigned long SND_HashName(const std::string& str)
{
	unsigned long hash = 5381;

	for (size_t x = 0; x < str.size(); x++)
	{
		hash = ((hash << 16) + (hash << 6) + std::tolower(str[x]) - hash) & 0xFFFFFFFF;
		if (hash == 0)
			hash = 1;
	}

	return hash;
}

// A stripped-down reimplementation of Stream_LoadFileSynchronously.
bool Stream_LoadFileSynchronously(streamInfo* stream) {
	uint64_t bytesRead = 0;

	if (stream->callbackOnly) {
		stream->genericCallback(stream->estMsToFinish, stream->callbackUser, stream->id);
		stream->genericCallback = nullptr;
		return true;
	}
	stream->bytes_read = 0;
	if (stream->buffer_size != 0) {
		if (s_fileIDs[stream->file].readOffset != stream->start_offset) {
			Stream_SeekInternal(s_fileIDs[stream->file].hFile, stream->start_offset);
			s_fileIDs[stream->file].readOffset = stream->start_offset;
		}
		if (!Stream_ReadInternal(s_fileIDs[stream->file].hFile, stream->start_offset, stream->destination, stream->buffer_size, 0, &bytesRead)) {
			return false;
		}
		s_fileIDs[stream->file].readOffset = s_fileIDs[stream->file].readOffset + bytesRead;
	}
	stream->bytes_read = bytesRead;
	stream->estMsToFinish = 0;
	return true;
}

// By sneakily hooking the game's sanity checker for all the SndAssetBankEntries, we can do our individual entry modifications here, and the game will be none the wiser :)
bool SND_AssetBankValidateEntryHook(uint32_t unk, SndAssetBankHeader* header, SndAssetBankEntry* entry) {

	// If we have a modded sound for the current entry, patch the file size and set the base offset.
	if (CollectedFiles.contains(entry->id)) {

		// Get the modded sound.
		ModdedSound& moddedSound = CollectedFiles.at(entry->id);

		std::cout << "[SND_AssetBankValidateEntry] Asset: " << moddedSound.relativeAssetPath << " has a NEW file size!\n";

		// Patch the file size of the SndAssetBankEntry.
		entry->size = std::filesystem::file_size(moddedSound.absoluteFilePath);

		// Set up the base offset of the modded sound, for use later on in our streamed-read hooks.
		moddedSound.baseOffset = entry->offset;
		return true;
	}

	// We don't have a modfile, so exit the function.
	return true;

}

// Helper function that returns the contents of register ESI.
__declspec(naked) streamInfo* GetESI() {
	__asm {
		mov eax, esi
		ret
	};
}

// Our ACTUAL streamed-read hook.
bool __cdecl Stream_LoadFileSynchronouslyHook() {
	// In order to do ANYTHING, we need to get the requested stream, which happens to always be in register ESI.
	// This is because, the function was (most-likely) inlined by the compiler since it only gets called once
	// in the stream thread function.
	auto stream = GetESI();

	// Declare and initialize the current SND alias to 0.
	uint32_t snd_hash = 0;


	// Here, we check for different stream callbacks to differentiate between different streaming read requests.
	// SNDs only use two of them, being SD_StreamReadCallback and SND_LoadAssetCallback. Music typically goes through
	// SD_StreamReadCallback, and sound effects typically go through SND_LoadAssetCallback. The reason why we need
	// to distinguish between them is because we have to acquire the currently requested SND alias by querying the
	// user data passed to the callbacks every read.  SD_StreamReadCallbacks take in a sd_stream_buffer, and
	// SND_LoadAssetCallbacks take in a SndAssetToLoad. Both of these structs are laid out very differently, but
	// both have a reference to the SND alias associated with the stream request.

	if (stream->callback == SD_StreamReadCallback) {
		if (stream->callbackUser != nullptr) {
			// Get the SND alias for the currently-requested read.
			auto stream_buffer = (sd_stream_buffer*)stream->callbackUser;
			snd_hash = stream_buffer->filenameHash;
		}
	}

	if (stream->callback == SND_LoadAssetCallback) {
		if (stream->callbackUser != nullptr) {
			// Get the SND alias for the currently-requested read.
			auto asset_to_load = (SndAssetToLoad*)stream->callbackUser;
			auto snd_hash = asset_to_load->assetId;
		}
	}

	// Make sure that we were in fact, able to get the SND alias.
	if (snd_hash != 0) {

		// If we have a modded sound for the requested read, continue onward.
		if (CollectedFiles.contains(snd_hash)) {

			// Get our modded sound file.
			auto& moddedSound = CollectedFiles.at(snd_hash);

			std::cout << "[Stream_LoadFileSynchronously] Loading SND: " << moddedSound.relativeAssetPath << "\n";

			// If the buffer size is 0, simply update the stream info and exit the function.
			if (stream->buffer_size == 0) {
				stream->estMsToFinish = 0;
				stream->bytes_read = 0;
				return true;
			}

			// Ensure that the streaming file handle's read offset is up-to-date.
			if (s_fileIDs[stream->file].readOffset != stream->start_offset) {
				s_fileIDs[stream->file].readOffset = stream->start_offset;
			}

			// Calculate the offset within loaded archive, and read the requested number of bytes.
			std::ifstream file(moddedSound.absoluteFilePath, std::ios::in | std::ios::binary);
			file.seekg(stream->start_offset - moddedSound.baseOffset, std::ios_base::beg);
			file.read(stream->destination, stream->buffer_size);
			file.close();

			// Update the streaming file handle and the stream info.
			s_fileIDs[stream->file].readOffset += stream->buffer_size;
			stream->estMsToFinish = 0;
			stream->bytes_read = stream->buffer_size;

			return true;
		}
	}

	// At this point, we know the requested file isn't ours at all. So we just call our reimplementation of the original function.
	return Stream_LoadFileSynchronously(stream);
}

void CollectFiles() {
	// Add sound\\patch to the current directory, and iterate over all files.
	std::filesystem::path patch_dir(std::filesystem::current_path().concat("\\sound\\patch\\"));
	if (std::filesystem::is_directory(patch_dir)) {
		for (const auto& entry : std::filesystem::recursive_directory_iterator(patch_dir)) {
			if (entry.is_regular_file()) {
				if (entry.path().extension().string() == ".flac") {
					// If the entry is a regular file, and has a .flac extension, then calculate the basename by getting its path relative to the patch directory, and stripping the last extension.
					auto base_filename = entry.path().lexically_relative(patch_dir).replace_extension().string();
					std::cout << "[CollectFiles] Found file: " << base_filename << ".\n";;

					// Insert a pair of the SND alias hash and a ModdedSound struct instance into the CollectedFiles hashmap.
					CollectedFiles.insert(
						{
							SND_HashName(base_filename),
							{
								entry.path(),
								base_filename,
								0x0, // The base offset will be modified later in the SND_AssetBankValidateEntry hook.
							},
						}
					);

				}
			}
		}
	}
}

HookedFunctionInfo streamingLoadHook_HFI;
HookedFunctionInfo validateEntryHook_HFI;
FILE* consoleHandle;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved) {

    if (reason == DLL_PROCESS_ATTACH) {
        // Initialize console and install hooks.
		AllocConsole();
		freopen_s(&consoleHandle, "CONOUT$", "w", stdout);
		SetConsoleTitleA("T6 Sound Replacement Plugin [itsmeft24]");

		// Collect all modded sounds held in "sound\\patch".
		CollectFiles();

		streamingLoadHook_HFI = HookFunction((void*&)Stream_LoadFileSynchronously_Original, &Stream_LoadFileSynchronouslyHook, 0xCB, FunctionHookType::EntireReplacement);
		validateEntryHook_HFI = HookFunction((void*&)SND_AssetBankValidateEntry_Original, &SND_AssetBankValidateEntryHook, 0x38, FunctionHookType::EntireReplacement);
    }
    else if (reason == DLL_PROCESS_DETACH) {
        // Deinitialize console and uninstall hooks.

		fclose(consoleHandle);
		FreeConsole();

        UninstallFunctionHook(streamingLoadHook_HFI);
        UninstallFunctionHook(validateEntryHook_HFI);
    }
	
    return TRUE;
}