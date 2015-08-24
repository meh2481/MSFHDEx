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
#include "FreeImage.h"
using namespace std;

//------------------------------
// .anim anim file structs
//------------------------------
typedef struct
{
	uint32_t sig;				//32 1E ED 32
	uint32_t unk;				//0
	uint32_t headerSz;			//38
	uint32_t numAnims;
	uint32_t animListOffset;	//point to animEntry[numAnims]
	uint32_t numFrames;
	uint32_t numListItems;
	uint32_t animListPtr;		//Point to animNumList[numFrames]
	uint32_t frameItemListPtr;	//Point to frameItemPtrList[numListItems]
	uint32_t imageDataPtr;		//point to imgHeader
	
	uint32_t spriteMinX;
	uint32_t spriteMinY;
	uint32_t spriteMaxX;
	uint32_t spriteMaxY;		//Probably
} animHeader;

typedef struct
{	
	uint32_t namePtr;	//Offset from start of file to animation name
	uint32_t nameHash_;	//Maybe?
	float	 unk1;		//1.0f
	uint32_t frameStart;	//Starting frame of animation (animNumList, that is)
	uint32_t frameEnd;		//Ending frame of animation
	int32_t  unk2[2];
	uint32_t unk3[2];
} animEntry;

typedef struct
{
	uint32_t sig;		//4E 68 B3 1E
	uint16_t unk;
	uint16_t numPieces;	//Number of images here
	uint32_t unk1;
	//followed by imgDataPtr[numPieces]
} imgHeader;

typedef struct
{
	uint32_t size;		//in the form of n, where 2^n is the width and height of the image
	uint32_t offset;	//in the form of x, where imgHeader pos + x = file offset to raw image data
} imgDataPtr;

typedef struct
{
	uint32_t frameItem;	//The frameItem number this frame uses (range: 0 to numListItems-1)
	int32_t  xOffset;	//Possibly; I don't have a better guess
	int32_t  yOffset;
} animNumList;

typedef struct
{
	uint32_t fiOffset;	//Point to frameItem
} frameItemPtrList;

typedef struct
{
	uint32_t sig;	//9A D4 70 E9, or šÔpé
	uint32_t unk1[5];
	uint32_t headerSz;	//36, or 0x24
	uint32_t numRects;			//Number of rectangles to follow
	uint32_t rectStartOffset;	//Point to rect[numRects] (offset from start of frameItem header)
	//Followed by rectImg[]
} frameItem;

typedef struct
{
	uint32_t img;		//Image number these rect(s) come from
	uint32_t rectStart;	//Starting rectangle for this image (rectStart = frameItem.numRects on rectImg sequence end)
} rectImg;

typedef struct
{
	//xy are always positive, uv are always negative
	//uv are big endian for some reason, and need to be bit-flipped
	uint16_t x, y;	//Final image coordinates, from bottom left of final image to bottom left of piece (+x is right, +y is up)
	int16_t  u, v;	//Starting image coordinates, out of -256 (u = -19 on a 128x128 image means 9 pixels from right side of image) -x is left, -y is up
} vec;

typedef struct
{
	vec bl, ul, ur, br;
} rect;	//One piece of an image


//------------------------------
// Helper structs
//------------------------------


typedef struct
{
	uint32_t img;
	rect rc;
} rectImgHelper;			//Cause rectImg is clunky to try to actually use


//------------------------------
// Helper functions
//------------------------------


int16_t bitFlip16(int16_t in)
{
	return ((int16_t)(in >> 8)|(int16_t)(in << 8));
}

void printVec(const vec& v)
{
	cout << v.x << "," << v.y << " - " << v.u << "," << v.v;
}

void printRect(const rect& rc)
{
	printVec(rc.ul);
	cout << "; ";
	printVec(rc.ur);
	cout << "; ";
	printVec(rc.br);
	cout << "; ";
	printVec(rc.bl);
}

void bitFlipRect(rect& r)
{
	r.ul.u = bitFlip16(r.ul.u);
	r.ul.v = bitFlip16(r.ul.v);
	r.ur.u = bitFlip16(r.ur.u);
	r.ur.v = bitFlip16(r.ur.v);
	r.bl.u = bitFlip16(r.bl.u);
	r.bl.v = bitFlip16(r.bl.v);
	r.br.u = bitFlip16(r.br.u);
	r.br.v = bitFlip16(r.br.v);
}

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
}


//------------------------------
// Main functions
//------------------------------


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
	cout << "Splitting images from file " << cFilename << endl;
	
	//Figure out what we'll be naming the images
	string sName = cFilename;
	//First off, strip off filename extension
	size_t namepos = sName.find(".anim");
	if(namepos != string::npos)
		sName.erase(namepos);
	//Next, strip off any file path before it
	namepos = sName.rfind('/');
	if(namepos == string::npos)
		namepos = sName.rfind('\\');
	if(namepos != string::npos)
		sName.erase(0, namepos+1);
		
	//grab header
	animHeader ah;
	memcpy(&ah, fileData, sizeof(animHeader));
	
	//grab animEntries
	vector<animEntry> animEntries;
	for(int i = 0; i < ah.numAnims; i++)
	{
		animEntry ae;
		memcpy(&ae, &fileData[ah.animListOffset+i*sizeof(animEntry)], sizeof(animEntry));
		
		animEntries.push_back(ae);
	}
	
	//Grab animNumLists
	vector<animNumList> animNums;
	for(int i = 0; i < ah.numFrames; i++)
	{
		animNumList anl;
		memcpy(&anl, &fileData[ah.animListPtr+i*sizeof(animNumList)], sizeof(animNumList));
		
		animNums.push_back(anl);
	}
	
	//Grab frameItemPtrLists
	vector<frameItemPtrList> fiPointers;
	for(int i = 0; i < ah.numListItems; i++)
	{
		frameItemPtrList fipl;
		memcpy(&fipl, &fileData[ah.animListPtr+i*sizeof(frameItemPtrList)+ah.numFrames*sizeof(animNumList)], sizeof(frameItemPtrList));
		
		fiPointers.push_back(fipl);
	}
	
	//Grab frameItems
	vector<frameItem> fiItems;
	vector< vector<rectImgHelper> > fiRects;
	for(int i = 0; i < ah.numListItems; i++)
	{
		frameItemPtrList fipl = fiPointers[i];
		frameItem fi;
		
		memcpy(&fi, &fileData[fipl.fiOffset], sizeof(frameItem));
		
		//cout << "FrameItem " << i << endl;
		vector<rect> fiRect;
		for(int j = 0; j < fi.numRects; j++)
		{
			rect r;
			memcpy(&r, &fileData[fipl.fiOffset+fi.rectStartOffset+j*sizeof(rect)], sizeof(rect));
			bitFlipRect(r);	//Make sure uv coordinates are correct...			
			
			//DEBUG: Figure out how these work...
			//printRect(r);
			//cout << endl;
			fiRect.push_back(r);
			
			//TODO: Handle rect rotation...
		}
		
		//Figure out what images these rects are from
		//Pull in rectImgs
		vector<rectImg> rcImgs;
		rectImg ri;
		ri.rectStart = 0;
		for(int j = 0; ri.rectStart < fi.numRects; j++)
		{
			memcpy(&ri, &fileData[fipl.fiOffset+sizeof(fi)+j*sizeof(rectImg)], sizeof(rectImg));
			rcImgs.push_back(ri);
		}
		
		//Fill in which rects go to which images
		vector<rectImgHelper> rih;
		int k = 0;
		//Loop per rectImg
		for(int j = 0; j < rcImgs.size()-1; j++)
		{
			//Pull all rects in from this image
			for(; k < rcImgs[j+1].rectStart; k++)
			{
				rectImgHelper help;
				help.img = rcImgs[j].img;
				help.rc = fiRect[k];
				rih.push_back(help);
			}
		}
		
		fiRects.push_back(rih);
		fiItems.push_back(fi);
	}
	
	/*/DEBUG: Make sure these are correct...
	for(int i = 0; i < fiRects.size(); i++)
	{
		cout << "Frame Item: " << i << endl;
		for(int j = 0; j < fiRects[i].size(); j++)
		{
			cout << "img " << fiRects[i][j].img << " rect: ";
			printRect(fiRects[i][j].rc);
			cout << endl;
		}
	}*/
	
	//Grab image header
	imgHeader ih;
	memcpy(&ih, &fileData[ah.imageDataPtr], sizeof(imgHeader));
	
	//Grab image data
	vector<FIBITMAP*> images;
	for(int i = 0; i < ih.numPieces; i++)
	{
		imgDataPtr idp;
		memcpy(&idp, &fileData[ah.imageDataPtr + sizeof(imgHeader) + i*sizeof(imgDataPtr)], sizeof(imgDataPtr));
		
		uint32_t imgSize = 1 << (idp.size);
		
		FIBITMAP* result = imageFromPixels(&fileData[ah.imageDataPtr + idp.offset], imgSize, imgSize);
		images.push_back(result);
		/*ostringstream oss;
		oss << "output/" << sName << '_' << i << ".png";
		cout << "Saving " << oss.str() << endl;
		
		FreeImage_Save(FIF_PNG, result, oss.str().c_str());
		FreeImage_Unload(result);*/
	}
	
	//Now we've got everything we need. Piece together some images!
	//TODO: Stitch together by animations. For now, we just want some images out so we know we did it correctly
	
	
	
	//Free leftover data
	for(vector<FIBITMAP*>::iterator i = images.begin(); i != images.end(); i++)
		FreeImage_Unload(*i);
	free(fileData);
	return true;
}

int main(int argc, char** argv)
{
	FreeImage_Initialise();
	//Create output folder	
#ifdef _WIN32
	CreateDirectory(TEXT("output"), NULL);
#else
	int result = system("mkdir -p output");
#endif
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
	FreeImage_DeInitialise();
	
	if(result)	//Pause if we ran into trouble, to give users time to see/copy down any error message...
	{
		cout << "Press Enter to exit..." << endl;
		cin.get();
	}
	
	return 0;
}
