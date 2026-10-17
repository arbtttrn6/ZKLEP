#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define SOUNDS_ROOT  "sounds"
#define OUTPUT_DIR   "output"
#define MAX_PATH_LEN 1024


struct SCharMapEntry
{
	const char* pszKey;
	const char* pszFile;
};

static const struct SCharMapEntry g_aCharMap[] =
{
	{ "zs", "zs" },
	{ "cz", "cz" },
	{ "sz", "sz" },
	{ "ja", "ja" },
	{ "ju", "ju" },
	{ " ",  "space" },

	{ "a", "a" }, { "b", "b" }, { "c", "c" }, { "d", "d" },
	{ "e", "e" }, { "f", "f" }, { "g", "g" }, { "h", "h" },
	{ "i", "i" }, { "j", "j" }, { "k", "k" }, { "l", "l" },
	{ "m", "m" }, { "n", "n" }, { "o", "o" }, { "p", "p" },
	{ "q", "q" }, { "r", "r" }, { "s", "s" }, { "t", "t" },
	{ "u", "u" }, { "v", "v" }, { "w", "w" }, { "x", "x" },
	{ "y", "y" }, { "z", "z" }
};

#define CHAR_MAP_SIZE ((int)(sizeof(g_aCharMap) / sizeof(g_aCharMap[0])))

struct SWavInfo
{
	unsigned short usFormat;
	unsigned short usChannels;
	unsigned int   uiSampleRate;
	unsigned short usBitsPerSample;
	unsigned int   uiDataSize;
	long           lDataOffset;
	long           lFileSize;
};


static unsigned int readU32LE(const unsigned char* p)
{
	return (unsigned int)p[0]
		 | ((unsigned int)p[1] << 8)
		 | ((unsigned int)p[2] << 16)
		 | ((unsigned int)p[3] << 24);
}


static unsigned short readU16LE(const unsigned char* p)
{
	return (unsigned short)((unsigned int)p[0] | ((unsigned int)p[1] << 8));
}


static void writeU32LE(FILE* fp, unsigned int uiVal)
{
	unsigned char a[4];
	a[0] = (unsigned char)( uiVal        & 0xFF);
	a[1] = (unsigned char)((uiVal >>  8) & 0xFF);
	a[2] = (unsigned char)((uiVal >> 16) & 0xFF);
	a[3] = (unsigned char)((uiVal >> 24) & 0xFF);
	fwrite(a, 1, 4, fp);
}


static void writeU16LE(FILE* fp, unsigned short usVal)
{
	unsigned char a[2];
	a[0] = (unsigned char)( usVal       & 0xFF);
	a[1] = (unsigned char)((usVal >> 8) & 0xFF);
	fwrite(a, 1, 2, fp);
}


static int parseWavHeader(const char* pszPath, struct SWavInfo* pInfo)
{
	FILE* fp;
	long lFileSize;
	unsigned char aHeader[12];
	unsigned char aChunk[8];
	int bHasFmt;
	int bHasData;

	fp = fopen(pszPath, "rb");

	if (!fp)
	{
		return 0;
	}

	if (fseek(fp, 0, SEEK_END) != 0)
	{
		fclose(fp);

		return 0;
	}

	lFileSize = ftell(fp);
	if (lFileSize < 0 || fseek(fp, 0, SEEK_SET) != 0)
	{
		fclose(fp);

		return 0;
	}

	if (lFileSize < 44)
	{
		fclose(fp);

		return 0;
	}

	if (fread(aHeader, 1, 12, fp) != 12 ||
		memcmp(aHeader, "RIFF", 4) != 0 ||
		memcmp(aHeader + 8, "WAVE", 4) != 0)
	{
		fclose(fp);

		return 0;
	}

	bHasFmt  = 0;
	bHasData = 0;

	while (ftell(fp) + 8 <= lFileSize)
	{
		unsigned int uiChunkSize;
		long lChunkDataPos;
		long lNext;

		if (fread(aChunk, 1, 8, fp) != 8)
		{
			break;
		}

		uiChunkSize   = readU32LE(aChunk + 4);
		lChunkDataPos = ftell(fp);

		if (memcmp(aChunk, "fmt ", 4) == 0)
		{
			unsigned char aFmt[16];
			if (uiChunkSize < 16)
			{
				fclose(fp);

				return 0;
			}

			if (fread(aFmt, 1, 16, fp) != 16)
			{
				fclose(fp);

				return 0;
			}

			pInfo->usFormat        = readU16LE(aFmt + 0);
			pInfo->usChannels      = readU16LE(aFmt + 2);
			pInfo->uiSampleRate    = readU32LE(aFmt + 4);
			pInfo->usBitsPerSample = readU16LE(aFmt + 14);
			bHasFmt = 1;
		}
		else if (memcmp(aChunk, "data", 4) == 0)
		{
			pInfo->uiDataSize  = uiChunkSize;
			pInfo->lDataOffset = lChunkDataPos;
			bHasData = 1;
		}

		lNext = lChunkDataPos + (long)uiChunkSize;
		if ((uiChunkSize & 1u) != 0)
		{
			lNext += 1;
		}

		if (fseek(fp, lNext, SEEK_SET) != 0)
		{
			break;
		}
	}

	pInfo->lFileSize = lFileSize;
	fclose(fp);

	return bHasFmt && bHasData;
}


static int readWavData(const char* pszPath, const struct SWavInfo* pInfo,
					   unsigned char** ppOut, unsigned int* puiOutSize)
{
	FILE* fp;
	unsigned char* pBuf;

	*ppOut      = NULL;
	*puiOutSize = 0;

	fp = fopen(pszPath, "rb");

	if (!fp)
	{
		return 0;
	}

	if (fseek(fp, pInfo->lDataOffset, SEEK_SET) != 0)
	{
		fclose(fp);

		return 0;
	}

	if (pInfo->uiDataSize == 0)
	{
		fclose(fp);

		return 1;
	}

	pBuf = (unsigned char*)malloc((size_t)pInfo->uiDataSize);
	if (!pBuf)
	{
		fclose(fp);

		return 0;
	}

	if (fread(pBuf, 1, pInfo->uiDataSize, fp) != pInfo->uiDataSize)
	{
		free(pBuf);
		fclose(fp);

		return 0;
	}

	fclose(fp);
	*ppOut      = pBuf;
	*puiOutSize = pInfo->uiDataSize;

	return 1;
}


static int joinPath(char* pszOut, int iOutSize,
					const char* pszA, const char* pszB)
{
	int iLenA;
	int iLenB;
	int bNeedSlash;
	int iTotal;
	int iPos;

	iLenA      = (int)strlen(pszA);
	iLenB      = (int)strlen(pszB);
	bNeedSlash = (iLenA > 0 &&
				  pszA[iLenA - 1] != '/' &&
				  pszA[iLenA - 1] != '\\');
	iTotal     = iLenA + (bNeedSlash ? 1 : 0) + iLenB;

	if (iTotal + 1 > iOutSize)
	{
		return 0;
	}

	memcpy(pszOut, pszA, (size_t)iLenA);
	iPos = iLenA;

	if (bNeedSlash)
	{
		pszOut[iPos++] = '/';
	}

	memcpy(pszOut + iPos, pszB, (size_t)iLenB);
	pszOut[iPos + iLenB] = '\0';

	return 1;
}

static int matchCharMap(const char* pszText)
{
	int iBest;
	size_t uiBestLen;
	int i;

	iBest     = -1;
	uiBestLen = 0;

	for (i = 0; i < CHAR_MAP_SIZE; ++i)
	{
		const char* pszKey = g_aCharMap[i].pszKey;
		size_t uiLen       = strlen(pszKey);

		if (uiLen == 0)
		{
			continue;
		}

		if (strncmp(pszText, pszKey, uiLen) == 0)
		{
			if (uiLen > uiBestLen)
			{
				uiBestLen = uiLen;
				iBest     = i;
			}
		}
	}

	return iBest;
}

static char* readWholeFile(const char* pszPath, long* plOutSize)
{
	FILE* fp;
	long lSize;
	char* pBuf;

	fp = fopen(pszPath, "rb");

	if (!fp)
	{
		return NULL;
	}

	if (fseek(fp, 0, SEEK_END) != 0)
	{
		fclose(fp);

		return NULL;
	}

	lSize = ftell(fp);
	if (lSize < 0 || fseek(fp, 0, SEEK_SET) != 0)
	{
		fclose(fp);

		return NULL;
	}

	pBuf = (char*)malloc((size_t)lSize + 1);
	if (!pBuf)
	{
		fclose(fp);

		return NULL;
	}

	if (lSize > 0)
	{
		if (fread(pBuf, 1, (size_t)lSize, fp) != (size_t)lSize)
		{
			free(pBuf);
			fclose(fp);

			return NULL;
		}
	}

	pBuf[lSize] = '\0';
	fclose(fp);
	*plOutSize = lSize;

	return pBuf;
}


const char* get_program_version(void)
{
	static char szFullVersion[17] = "1.0.0.00000000";
	const char* pszD;
	const char* pszM;
	int nMonth;
	int nI;

	if (szFullVersion[6] == '0' && szFullVersion[7] == '0')
	{
		pszD = __DATE__;

		szFullVersion[6] = (pszD[4] == ' ') ? '0' : pszD[4];
		szFullVersion[7] = pszD[5];

		pszM   = "JanFebMarAprMayJunJulAugSepOctNovDec";
		nMonth = 1;
		for (nI = 0; nI < 12; ++nI)
		{
			if (pszD[0] == pszM[nI * 3] && pszD[1] == pszM[nI * 3 + 1])
			{
				nMonth = nI + 1;
				break;
			}
		}
		szFullVersion[8]  = (char)('0' + nMonth / 10);
		szFullVersion[9]  = (char)('0' + nMonth % 10);
		szFullVersion[10] = pszD[7];
		szFullVersion[11] = pszD[8];
		szFullVersion[12] = pszD[9];
		szFullVersion[13] = pszD[10];
	}

	return szFullVersion;
}

void PrintInfo(const char* pszProgramName)
{
	const char* pszProgramVersion = get_program_version();
	printf("author: Artur Ajvazjan (arbtttrn6)\n");
	printf("version: %s\n", pszProgramVersion);
	printf("usage:\n");
	printf("  %s <string.txt> [name_of_sound_bank] [name_of_output_file.wav]\n",
		   pszProgramName);
}


int main(int argc, char* argv[])
{
	const char* pszInput;
	const char* pszBank;
	const char* pszOut;
	long lTextSize = 0;
	char* pszText;
	char szBankDir[MAX_PATH_LEN];
	unsigned char* pAllData;
	unsigned int uiAllSize;
	unsigned int uiAllCap;
	struct SWavInfo infoRef;
	int bHaveRef;
	int bOk;
	const char* psz;
	FILE* fpOut;
	char szOutPath[MAX_PATH_LEN];
	unsigned int uiDataSize;
	unsigned int uiByteRate;
	unsigned short usBlockAlign;
	unsigned int uiRiffSize;

	if (argc < 2)
	{
		PrintInfo(argv[0]);

		return 1;
	}

	pszInput = argv[1];
	pszBank  = (argc >= 3) ? argv[2] : "bank1";
	pszOut   = (argc >= 4) ? argv[3] : "out.wav";

	pszText = readWholeFile(pszInput, &lTextSize);
	if (!pszText)
	{
		fprintf(stderr, "Cannot open input: %s\n", pszInput);

		return 2;
	}

	if (!joinPath(szBankDir, MAX_PATH_LEN, SOUNDS_ROOT, pszBank))
	{
		fprintf(stderr, "Bank path too long\n");
		free(pszText);

		return 2;
	}

	pAllData   = NULL;
	uiAllSize  = 0;
	uiAllCap   = 0;
	bHaveRef   = 0;
	bOk        = 1;

	psz = pszText;
	while (*psz != '\0')
	{
		int iIdx;
		const struct SCharMapEntry* pEntry;
		char szFileName[MAX_PATH_LEN];
		char szWavPath[MAX_PATH_LEN];
		size_t uiNameLen;
		struct SWavInfo info;
		unsigned char* pChunk;
		unsigned int uiChunkSize;
		int bNameOk;

		iIdx = matchCharMap(psz);
		if (iIdx < 0)
		{
			fprintf(stderr, "Skip unknown char at pos\n");
			++psz;

			continue;
		}

		pEntry = &g_aCharMap[iIdx];

		uiNameLen = strlen(pEntry->pszFile);
		bNameOk   = (uiNameLen + 4 + 1 <= (size_t)MAX_PATH_LEN);
		if (bNameOk)
		{
			memcpy(szFileName, pEntry->pszFile, uiNameLen);
			memcpy(szFileName + uiNameLen, ".wav", 4);
			szFileName[uiNameLen + 4] = '\0';
		}
		if (!bNameOk)
		{
			fprintf(stderr, "File name too long\n");
			bOk = 0;

			break;
		}

		if (!joinPath(szWavPath, MAX_PATH_LEN, szBankDir, szFileName))
		{
			fprintf(stderr, "Path too long: %s\n", szFileName);
			bOk = 0;

			break;
		}

		if (!parseWavHeader(szWavPath, &info))
		{
			fprintf(stderr, "Cannot parse WAV: %s\n", szWavPath);
			bOk = 0;

			break;
		}

		if (!bHaveRef)
		{
			infoRef  = info;
			bHaveRef = 1;
		}
		else
		{
			if (info.usFormat        != infoRef.usFormat ||
				info.usChannels      != infoRef.usChannels ||
				info.uiSampleRate    != infoRef.uiSampleRate ||
				info.usBitsPerSample != infoRef.usBitsPerSample)
			{
				fprintf(stderr, "Format mismatch: %s\n", szWavPath);
				bOk = 0;

				break;
			}
		}

		pChunk     = NULL;
		uiChunkSize = 0;
		if (!readWavData(szWavPath, &info, &pChunk, &uiChunkSize))
		{
			fprintf(stderr, "Cannot read data: %s\n", szWavPath);
			bOk = 0;

			break;
		}

		if (uiChunkSize > 0)
		{
			if (uiAllSize > 0xFFFFFFFFu - uiChunkSize)
			{
				fprintf(stderr, "Total size overflow\n");
				free(pChunk);
				bOk = 0;

				break;
			}

			if (uiAllSize + uiChunkSize > uiAllCap)
			{
				unsigned int uiNewCap = (uiAllCap > 0) ? uiAllCap : 4096;
				unsigned char* pNew;

				while (uiNewCap < uiAllSize + uiChunkSize)
				{
					if (uiNewCap > 0xFFFFFFFFu / 2u)
					{
						uiNewCap = uiAllSize + uiChunkSize;

						break;
					}
					uiNewCap *= 2;
				}

				pNew = (unsigned char*)realloc(pAllData, (size_t)uiNewCap);
				if (!pNew)
				{
					fprintf(stderr, "Out of memory\n");
					free(pChunk);
					bOk = 0;

					break;
				}

				pAllData = pNew;
				uiAllCap = uiNewCap;
			}

			memcpy(pAllData + uiAllSize, pChunk, (size_t)uiChunkSize);
			uiAllSize += uiChunkSize;
		}
		free(pChunk);

		psz += strlen(pEntry->pszKey);
	}

	free(pszText);
	pszText = NULL;

	if (!bOk || !bHaveRef)
	{
		free(pAllData);
		fprintf(stderr, "Nothing to write or error occurred.\n");

		return 3;
	}

	if (!joinPath(szOutPath, MAX_PATH_LEN, OUTPUT_DIR, pszOut))
	{
		free(pAllData);
		fprintf(stderr, "Output path too long\n");

		return 4;
	}

	fpOut = fopen(szOutPath, "wb");
	if (!fpOut)
	{
		free(pAllData);
		fprintf(stderr, "Cannot create output: %s\n", szOutPath);

		return 4;
	}

	uiDataSize    = uiAllSize;
	uiByteRate    = infoRef.uiSampleRate
				  * (unsigned int)infoRef.usChannels
				  * (unsigned int)infoRef.usBitsPerSample / 8u;
	usBlockAlign  = (unsigned short)((unsigned int)infoRef.usChannels
				  * (unsigned int)infoRef.usBitsPerSample / 8u);
	uiRiffSize    = 36u + uiDataSize;

	fwrite("RIFF", 1, 4, fpOut);
	writeU32LE(fpOut, uiRiffSize);
	fwrite("WAVE", 1, 4, fpOut);

	fwrite("fmt ", 1, 4, fpOut);
	writeU32LE(fpOut, 16u);
	writeU16LE(fpOut, 1u); /* PCM */
	writeU16LE(fpOut, infoRef.usChannels);
	writeU32LE(fpOut, infoRef.uiSampleRate);
	writeU32LE(fpOut, uiByteRate);
	writeU16LE(fpOut, usBlockAlign);
	writeU16LE(fpOut, infoRef.usBitsPerSample);

	fwrite("data", 1, 4, fpOut);
	writeU32LE(fpOut, uiDataSize);
	if (uiDataSize > 0)
	{
		fwrite(pAllData, 1, uiDataSize, fpOut);
	}

	if (ferror(fpOut))
	{
		fclose(fpOut);
		free(pAllData);
		fprintf(stderr, "Write error\n");

		return 5;
	}

	fclose(fpOut);
	free(pAllData);

	printf("Written: %s (%u bytes of data)\n", szOutPath, uiDataSize);

	return 0;
}
