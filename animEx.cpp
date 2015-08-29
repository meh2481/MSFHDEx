#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <stdlib.h>
#include <cstdio>
#ifdef _WIN32
	#include <windows.h>
	//#include <Shlobj.h>
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
	uint32_t item;	//The frameItem number this frame uses (range: 0 to numListItems-1)
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
	uint16_t x, y;	//Final image coordinates, from bottom left of final image to bottom left of piece (+x is right, +y is up)
	uint16_t u, v;	//Starting image coordinates, out of 0xFFFF (divide then multiply by image size to get coordinates)
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
	float rotAmt;
	bool bFlip;
	uint32_t img;
	rect rc;
} rectImgHelper;			//Cause rectImg is clunky to try to actually use


//------------------------------
// Helper functions
//------------------------------
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

void fixVec(vec& v, uint32_t finalW, uint32_t finalH, int32_t imgSize)//, bool bRoundX, bool bRoundY)
{
	v.y = finalH - v.y;
	
	double divee = 0xFFFF;
	double divX = v.u;
	double divY = v.v;
	double sz = imgSize;
	v.u = (divX / divee * imgSize)+0.5;
	v.v = (divY / divee * imgSize)+0.5;
}

void fixRect(rectImgHelper& rcHelp, uint32_t finalW, uint32_t finalH, int32_t imgSize)
{
	//Convert these to actual pixel values
	fixVec(rcHelp.rc.ul, finalW, finalH, imgSize);
	fixVec(rcHelp.rc.ur, finalW, finalH, imgSize);
	fixVec(rcHelp.rc.bl, finalW, finalH, imgSize);
	fixVec(rcHelp.rc.br, finalW, finalH, imgSize);
	
	//Rotate as needed
	rcHelp.rotAmt = 0;
	rcHelp.bFlip = false;
	
	if(rcHelp.rc.ur.u == rcHelp.rc.ul.u && rcHelp.rc.ur.v > rcHelp.rc.ul.v)	//Image is rotated CCW, flipped horiz
	{
		//Fix rect, and mark for rot
		rcHelp.rotAmt = -90;
		rcHelp.bFlip = true;
		
		//Flip ur and bl
		uint16_t tempX, tempY;
		tempX = rcHelp.rc.bl.u;
		tempY = rcHelp.rc.bl.v;
		rcHelp.rc.bl.u = rcHelp.rc.ur.u;
		rcHelp.rc.bl.v = rcHelp.rc.ur.v;
		rcHelp.rc.ur.u = tempX;
		rcHelp.rc.ur.v = tempY;
	}
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
		
		vector<rect> fiRect;
		for(int j = 0; j < fi.numRects; j++)
		{
			rect r;
			memcpy(&r, &fileData[fipl.fiOffset+fi.rectStartOffset+j*sizeof(rect)], sizeof(rect));
			
			fiRect.push_back(r);
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
		//ostringstream oss;
		//oss << "output/" << sName << "data_" << i << ".png";
		//cout << "Saving " << oss.str() << endl;
		
		//FreeImage_Save(FIF_PNG, result, oss.str().c_str());
		//FreeImage_Unload(result);
	}
	
	//Now we've got everything we need. Piece together some images!
	//TODO: Stitch together by animations. For now, we just want some images out so we know we did it correctly
	
	
	//vec sz;
	//sz.x = sz.y = 0;
	//sz.x = -ah.spriteMinX + ah.spriteMaxX;
	//sz.y = -ah.spriteMinY + ah.spriteMaxY;
	
	//Convert rectangles to proper format
	vector<vec> imgFinalSizes;
	for(int i = 0; i < fiRects.size(); i++)
	{
		//Get final image w/h for this
		vec sz;
		sz.x = sz.y = 0;
		for(int j = 0; j < fiRects[i].size(); j++)
		{
			sz.x = max(sz.x, (uint16_t)(fiRects[i][j].rc.bl.x + 1));
			sz.x = max(sz.x, (uint16_t)(fiRects[i][j].rc.br.x + 1));
			sz.x = max(sz.x, (uint16_t)(fiRects[i][j].rc.ul.x + 1));
			sz.x = max(sz.x, (uint16_t)(fiRects[i][j].rc.ur.x + 1));
			sz.y = max(sz.y, (uint16_t)(fiRects[i][j].rc.bl.y + 1));
			sz.y = max(sz.y, (uint16_t)(fiRects[i][j].rc.br.y + 1));
			sz.y = max(sz.y, (uint16_t)(fiRects[i][j].rc.ul.y + 1));
			sz.y = max(sz.y, (uint16_t)(fiRects[i][j].rc.ur.y + 1));
		}
		
		//Now that we have the sizes, we can fix these rectangles to point to actual image locations
		
		for(int j = 0; j < fiRects[i].size(); j++)
		{
			fixRect(fiRects[i][j], sz.x, sz.y, FreeImage_GetWidth(images[fiRects[i][j].img]));
		}
		imgFinalSizes.push_back(sz);
	}
	
	//Stitch final images
	vector<FIBITMAP*> stitchedImages;
	for(int i = 0; i < imgFinalSizes.size(); i++)
	{
		//Create final image
		FIBITMAP* result = FreeImage_Allocate(imgFinalSizes[i].x, imgFinalSizes[i].y, 32);
		
		//Piece images
		for(int j = 0; j < fiRects[i].size(); j++)
		{
			//Grab piece
			FIBITMAP* imgPiece = FreeImage_Copy(images[fiRects[i][j].img], fiRects[i][j].rc.ul.u, fiRects[i][j].rc.ul.v, fiRects[i][j].rc.br.u, fiRects[i][j].rc.br.v);
			//If this needs to be rotated
			if(fiRects[i][j].rotAmt)
			{
				FIBITMAP* tmp = imgPiece;
				imgPiece = FreeImage_Rotate(imgPiece, fiRects[i][j].rotAmt);
				FreeImage_Unload(tmp);
			}
			//If this needs to be flipped
			if(fiRects[i][j].bFlip)
				FreeImage_FlipHorizontal(imgPiece);
			//Paste this to the proper location in the destination image
			FreeImage_Paste(result, imgPiece, fiRects[i][j].rc.ul.x, fiRects[i][j].rc.ul.y, 255);
			FreeImage_Unload(imgPiece);
		}
		
		stitchedImages.push_back(result);
		//Save the image
		//ostringstream oss;
		//oss << "output/" << sName << '_' << i << ".png";
		//cout << "Saving " << oss.str() << endl;
		
		//FreeImage_Save(FIF_PNG, result, oss.str().c_str());
		//FreeImage_Unload(result);
	}
	
	//Spit out by animation
	for(int i = 0; i < ah.numAnims; i++)
	{
		//Find the extents for the images in this anim
		int left, right, top, bottom;
		int totalXOffset, totalYOffset;
		left = right = top = bottom = totalXOffset = totalYOffset = 0;
		for(int j = animEntries[i].frameStart; j < animEntries[i].frameEnd; j++)
		{
			//animNums[j].item
			//-y is down for these calculations, as it is in the .anim files
			if(left > -animNums[j].xOffset)
				left = totalXOffset = -animNums[j].xOffset;
			if(right < imgFinalSizes[animNums[j].item].x - animNums[j].xOffset)
				right = imgFinalSizes[animNums[j].item].x - animNums[j].xOffset;
			if(bottom > -animNums[j].yOffset)
				bottom = totalYOffset = -animNums[j].yOffset;
			if(top < imgFinalSizes[animNums[j].item].y - animNums[j].yOffset)
				top = imgFinalSizes[animNums[j].item].y - animNums[j].yOffset;
		}		
		
		const char* cAnimName = (const char*)(&fileData[animEntries[i].namePtr]);
		int curFrame = 1;
		for(int j = animEntries[i].frameStart; j < animEntries[i].frameEnd; j++)
		{
			//Save the image
			ostringstream oss;
			oss << "output/" << sName;
			CreateDirectory(TEXT(oss.str().c_str()), NULL);
			oss << '/' << cAnimName << '/';
			CreateDirectory(TEXT(oss.str().c_str()), NULL);
			//SHCreateDirectoryEx( NULL, oss.str().c_str(), NULL );
			oss << setw(3) << setfill('0') << curFrame++ << ".png";
			//cout << "(Offset:) " << animNums[j].xOffset << ", " << animNums[j].yOffset << endl;
			cout << "Saving " << oss.str() << endl;
			
			//Allocate the final image
			FIBITMAP* finalFrameImg = FreeImage_Allocate(right-left, top-bottom, 32);
			FreeImage_Paste(finalFrameImg, 
							stitchedImages[animNums[j].item], 
							-totalXOffset - animNums[j].xOffset, 
							FreeImage_GetHeight(finalFrameImg) - imgFinalSizes[animNums[j].item].y + animNums[j].yOffset + totalYOffset, 
							255);
			
			//TODO: Stitch these into sheets
			FreeImage_Save(FIF_PNG, finalFrameImg, oss.str().c_str());
			FreeImage_Unload(finalFrameImg);
		}
	}
	
	
	//Free leftover data
	for(vector<FIBITMAP*>::iterator i = images.begin(); i != images.end(); i++)
		FreeImage_Unload(*i);
	for(vector<FIBITMAP*>::iterator i = stitchedImages.begin(); i != stitchedImages.end(); i++)
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
