#pragma once

#pragma pack(push, 1)

// Copied from OpenBO2
struct SndAssetBankHeader
{
	unsigned int magic;
	unsigned int version;
	unsigned int entrySize;
	unsigned int checksumSize;
	unsigned int dependencySize;
	unsigned int entryCount;
	unsigned int dependencyCount;
	unsigned int pad32;
	__int64 fileSize;
	__int64 entryOffset;
	__int64 checksumOffset;
	unsigned __int8 checksumChecksum[16];
	char dependencies[512];
	char padding[1464];
};

// Copied from OpenBO2
struct SndAssetBankEntry
{
	unsigned int id;
	unsigned int size;
	unsigned int offset;
	unsigned int frameCount;
	unsigned __int8 frameRateIndex;
	unsigned __int8 channelCount;
	unsigned __int8 looping;
	unsigned __int8 format;
};

// Copied from OpenBO2
struct stream_fh
{
	char name[256];
	HANDLE hFile;
	bool inUse;
	bool shouldOpen;
	bool shouldClose;
	bool error;
	int flags;
	int extAsInt;
	unsigned int readOffset;
	unsigned int easyOffset;
	__int64 fileSize;
};

// Copied from OpenBO2
struct SndAssetToLoad
{
	int bankFileId;
	int fileOffset;
	int size;
	void* destination;
	unsigned int memoryOffset;
	unsigned int sndLoadState;
	unsigned int assetId;
	void* Sndbank;
	int streamRequestId;
	unsigned int loadId;
};

// Copied from OpenBO2
enum stream_status
{
	STREAM_STATUS_INVALID = 0x0,
	STREAM_STATUS_QUEUED = 0x1,
	STREAM_STATUS_INPROGRESS = 0x2,
	STREAM_STATUS_CANCELLED = 0x3,
	STREAM_STATUS_DEVICE_REMOVED = 0x4,
	STREAM_STATUS_READFAILED = 0x5,
	STREAM_STATUS_EOF = 0x6,
	STREAM_STATUS_FINISHED = 0x7,
	STREAM_STATUS_USER1 = 0x8,
	STREAM_STATUS_COUNT = 0x9,
};

// Copied from OpenBO2
struct streamInfo
{
	int id;
	int file;
	unsigned int start_offset;
	unsigned int buffer_size;
	unsigned int bytes_read;
	char* destination;
	int estMsToFinish;
	int startDeadline;
	union {
		void(__cdecl* genericCallback)(int, void*, int);
		void(__cdecl* callback)(int, stream_status, unsigned int, void*);
	};
	void* callbackUser;
	stream_status status;
	bool callbackOnly;
	streamInfo* nextInQueue;
	streamInfo* prevInQueue;
};

// Copied from OpenBO2
struct sd_stream_buffer
{
	volatile int refCount;
	const char* filename;
	unsigned int filenameHash;
	unsigned int offset;
	unsigned int readSize;
	unsigned int requestLatency;
	unsigned int requestStartTime;
	unsigned int requestEndTime;
	int requestId;
	char* data;
	int valid;
	int error;
	int primed;
	int preloadExpires;
};

#pragma pack(pop)

// Copied from OpenBO2
// The game's fixed-size array of all the streaming file handles. Its size is always 64 elements.
stream_fh* s_fileIDs = (stream_fh*)0x028785A0;
