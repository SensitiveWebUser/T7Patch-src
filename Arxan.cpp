#include "Arxan.h"

// Builds this patch's offsets were derived from. Every RVA in Arxan.h and offsets.h is specific to
// one BlackOps3.exe build. Applied to any other they land on unrelated code or unmapped memory,
// which is a hard crash during startup, before the patch's own exception handler is installed.
// Add an entry here only after re-verifying the offsets against that build.
struct SupportedBuild
{
	DWORD timeDateStamp;       // IMAGE_FILE_HEADER.TimeDateStamp, changes on every relink
	DWORD sizeOfImage;         // IMAGE_OPTIONAL_HEADER64.SizeOfImage
	DWORD addressOfEntryPoint; // IMAGE_OPTIONAL_HEADER64.AddressOfEntryPoint
};

static const SupportedBuild kSupportedBuilds[] =
{
	{ 0x693D731E, 0x1D74B000, 0x02BD40FC }, // retail Steam build, linked 2025-12-13
};

// Highest RVA PatchChecksumComparisons_Precomputed writes to, across all three tables.
static const DWORD kHighestPatchedRva = 0x1D6692FA;

// Reads the three identifying header fields of the loaded game image. Returns false if the headers
// are not a well-formed 64-bit PE, in which case the out params are left zeroed.
static bool ReadGameBuildIds(DWORD& timeDateStamp, DWORD& sizeOfImage, DWORD& entryPoint)
{
	timeDateStamp = 0;
	sizeOfImage = 0;
	entryPoint = 0;

	auto base = (const BYTE*)GetModuleHandleA(NULL);
	if (!base)
	{
		return false;
	}

	auto dos = (const IMAGE_DOS_HEADER*)base;
	if (dos->e_magic != IMAGE_DOS_SIGNATURE)
	{
		return false;
	}

	auto nt = (const IMAGE_NT_HEADERS64*)(base + dos->e_lfanew);
	if (nt->Signature != IMAGE_NT_SIGNATURE || nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC)
	{
		return false;
	}

	timeDateStamp = nt->FileHeader.TimeDateStamp;
	sizeOfImage = nt->OptionalHeader.SizeOfImage;
	entryPoint = nt->OptionalHeader.AddressOfEntryPoint;
	return true;
}

static bool VerifyGameBuild()
{
	DWORD timeDateStamp = 0, sizeOfImage = 0, entryPoint = 0;
	if (!ReadGameBuildIds(timeDateStamp, sizeOfImage, entryPoint))
	{
		return false;
	}

	// Structural floor, independent of any known build: every address we are about to write must
	// lie inside the image. This alone stops the write-to-unmapped-memory crash on a wrong exe.
	if (sizeOfImage <= kHighestPatchedRva + 16)
	{
		return false;
	}

	for (const auto& build : kSupportedBuilds)
	{
		if (timeDateStamp == build.timeDateStamp
			&& sizeOfImage == build.sizeOfImage
			&& entryPoint == build.addressOfEntryPoint)
		{
			return true;
		}
	}

	return false;
}

void PatchAddress(uint8_t* address, const uint8_t* patch, size_t patchSize)
{
	DWORD oldProtect = 0;

	// Fail closed. VerifyGameBuild above should already have rejected an unexpected exe, but this is
	// 1365 blind writes running before the patch's own exception handler is installed, so a failed
	// VirtualProtect must not be followed by the write anyway.
	if (!VirtualProtect(address, patchSize, PAGE_EXECUTE_READWRITE, &oldProtect))
		return;

	for (size_t i = 0; i < patchSize; i++)
		address[i] = patch[i];

	FlushInstructionCache(GetCurrentProcess(), address, patchSize);

	DWORD dummy;
	VirtualProtect(address, patchSize, oldProtect, &dummy);
}


template <size_t N>
struct ObfuscatedPatch {
	std::array<uint8_t, N> data;
	static constexpr uint8_t key = 0x69;

	constexpr ObfuscatedPatch(const uint8_t(&input)[N]) : data{} {
		for (size_t i = 0; i < N; ++i) {
			data[i] = input[i] ^ key;
		}
	}

	void Apply(uint8_t* address, void (*patchFunc)(uint8_t*, const uint8_t*, size_t)) const {
		uint8_t decrypted[N];
		for (size_t i = 0; i < N; ++i) {
			decrypted[i] = data[i] ^ key;
		}
		patchFunc(address, decrypted, N);

		memset(decrypted, 0, N);
	}
};

static constexpr ObfuscatedPatch patch_crc_1({ 0x48, 0x31, 0xC9, 0x90, 0x90, 0x90 });
static constexpr ObfuscatedPatch patch_crc_2({ 0x48, 0x31, 0xC9, 0x90, 0x90, 0x90, 0x90, 0x90 });
static constexpr ObfuscatedPatch patch_crc_3({ 0x48, 0x31, 0xC0, 0x48, 0x31, 0xD2 });

static bool g_gameBuildSupported = false;

bool IsSupportedGameBuild()
{
	return g_gameBuildSupported;
}

bool PatchChecksumComparisons_Precomputed()
{
	if (!VerifyGameBuild())
	{
		// Leads with "the game updated" rather than "your files are broken", since verifying
		// files would just re-download the new build. Prints the observed values so a
		// screenshot is enough to add the build to kSupportedBuilds.
		char message[512]{};
		DWORD timeDateStamp = 0, sizeOfImage = 0, entryPoint = 0;
		ReadGameBuildIds(timeDateStamp, sizeOfImage, entryPoint);

		sprintf_s(message,
			"T7Patch does not support this Black Ops III build, so it has not loaded.\n\n"
			"This usually means the game was updated. Check for a new version of T7Patch - "
			"verifying your game files will not help.\n\n"
			"Please include these values in any report:\n"
			"  TimeDateStamp: 0x%08X\n"
			"  SizeOfImage:   0x%08X\n"
			"  EntryPoint:    0x%08X",
			timeDateStamp, sizeOfImage, entryPoint);

		ZLOG("arxan: UNSUPPORTED BUILD - patches NOT applied. TimeDateStamp=%08X SizeOfImage=%08X EntryPoint=%08X",
			timeDateStamp, sizeOfImage, entryPoint);
		MessageBoxA(nullptr, message, "T7Patch",
			MB_OK | MB_ICONERROR | MB_TOPMOST | MB_SETFOREGROUND);
		return false;
	}

	g_gameBuildSupported = true;

	for (auto rva : crc_patch_1)
		patch_crc_1.Apply((uint8_t*)(REBASE(rva)), PatchAddress);

	for (auto rva : crc_patch_2)
		patch_crc_2.Apply((uint8_t*)(REBASE(rva)), PatchAddress);

	for (auto rva : crc_patch_3)
		patch_crc_3.Apply((uint8_t*)(REBASE(rva)), PatchAddress);

	return true;
}