#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <stdlib.h>
#include <cstdio>
#ifdef _WIN32
	#include <windows.h>
#endif
#include <list>
#include <cmath>
#include <cstring>
#include <iomanip>
using namespace std;

//------------------------------
// .vol volume archive structs
//------------------------------
typedef struct
{
	uint32_t unk1[3];
	uint32_t numFiles;
	uint32_t headerSz;
} volHeader;

typedef struct
{
	uint32_t nameOffset;
	uint32_t dataOffset1;
	uint32_t dataOffset2;
	uint32_t dataSz;
	uint32_t unk;
} volEntry;

/*typedef struct
{
	uint32_t unk[7];
	uint32_t numDictEntries;
} list1;


FIBITMAP* imageFromPixels(uint8_t* imgData, uint32_t width, uint32_t height)
{
	//FreeImage is broken here and you can't swap R/G/B channels upon creation. Do that manually
	FIBITMAP* result = FreeImage_ConvertFromRawBits(imgData, width, height, ((((32 * width) + 31) / 32) * 4), 32, FI_RGBA_RED, FI_RGBA_GREEN, FI_RGBA_BLUE, true);
	FIBITMAP* r = FreeImage_GetChannel(result, FICC_RED);
	FIBITMAP* b = FreeImage_GetChannel(result, FICC_BLUE);
	FreeImage_SetChannel(result, b, FICC_RED);
	FreeImage_SetChannel(result, r, FICC_BLUE);
	FreeImage_Unload(r);
	FreeImage_Unload(b);
	return result;
}*/


bool splitFiles(const char* cFilename)
{
	uint8_t* fileData;
	FILE* fh = fopen(cFilename, "rb");
	if(fh == NULL)
	{
		cerr << "Unable to open input file " << cFilename << endl;
		return false;
	}
	fseek(fh, 0, SEEK_END);
	size_t fileSize = ftell(fh);
	fseek(fh, 0, SEEK_SET);
	fileData = (uint8_t*)malloc(fileSize);
	size_t amt = fread(fileData, fileSize, 1, fh);
	fclose(fh);
	cout << "Splitting files from volume " << cFilename << endl;
	
	//Figure out what we'll be naming the images
	string sName = cFilename;
	//First off, strip off filename extension
	size_t namepos = sName.find(".vol");
	if(namepos != string::npos)
		sName.erase(namepos);
	//Next, strip off any file path before it
	namepos = sName.rfind('/');
	if(namepos == string::npos)
		namepos = sName.rfind('\\');
	if(namepos != string::npos)
		sName.erase(0, namepos+1);
		
	//Create output folder	
#ifdef _WIN32
	CreateDirectory(TEXT(sName.c_str()), NULL);
#else
	int result = system("mkdir -p output");
#endif
		
	//grab header
	volHeader vh;
	memcpy(&vh, fileData, sizeof(volHeader));
	
	//grab files
	for(int i = 0; i < vh.numFiles; i++)
	{
		/*typedef struct
{
	uint32_t nameOffset;
	uint64_t dataOffset;
	uint32_t dataSz;
	uint32_t unk;
} volEntry;*/

		volEntry ve;
		memcpy(&ve, &fileData[vh.headerSz+4+i*sizeof(volEntry)], sizeof(volEntry));
		
		cout << std::hex << ve.nameOffset << ", " << ve.dataOffset1 << ", " << ve.dataOffset2 << ", " << ve.dataSz << endl;
		
		ostringstream oss;
		oss << sName << "/" << (char*)(&fileData[ve.nameOffset]);
		cout << "Saving " << oss.str() << endl;
		
		FILE* f = fopen(oss.str().c_str(), "wb");
		uint64_t dataOff = (uint64_t)ve.dataOffset1 | ((uint64_t)ve.dataOffset2 << 32);
		fwrite(&fileData[dataOff], 1, ve.dataSz, f);
		fclose(f);
	}
		
	//Grab data Header
	//DataHeader dh;
	//memcpy(&dh, fileData, sizeof(DataHeader));
	
	//TODO
	//int oSize = 0;
	//byte* output = lzx_decompress(&fileData[dh.binData1Offset], &oSize);
	
	//FILE* f = fopen("out.tmp", "wb");
	//fwrite(output, oSize, 1, f);
	//fclose(f);
	
	//delete[] output;
	
	return true;
}

int main(int argc, char** argv)
{
	//FreeImage_Initialise();
	
	list<string> sFilenames;
	//Parse commandline
	for(int i = 1; i < argc; i++)
	{
		string s = argv[i];
		sFilenames.push_back(s);
	}
	//Decompress data files
	int result = 0;
	for(list<string>::iterator i = sFilenames.begin(); i != sFilenames.end(); i++)
	{
		if(!splitFiles((*i).c_str()))
			result = 1;
	}
	//FreeImage_DeInitialise();
	
	if(result)	//Pause if we ran into trouble, to give users time to see/copy down any error message...
	{
		cout << "Press Enter to exit..." << endl;
		cin.get();
	}
	
	return 0;
}
