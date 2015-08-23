//------------------------------
// .anim anim file structs
//------------------------------
typedef struct
{
	uint32_t unk1[2];
	uint32_t numFrames_;	//?
	uint32_t numAnims;
	uint32_t animListOffset;	//point to list of numAnims animEntries
	uint32_t unk2[2];
	uint32_t nextPtr;	//Point to ???? after anim names
	uint32_t somePtr_;	//Maybe a pointer to something else?
	uint32_t imageDataPtr;	//point to imgHeader
} animHeader;

typedef struct
{	
	uint32_t namePtr;
	uint32_t unk[8];
} animEntry;

typedef struct
{
	uint32_t sig;	//45854
	uint16_t unk;
	uint16_t numPieces;
	uint32_t unk1;
	//followed by imgDataPtr[numPieces]
} imgHeader;

typedef struct
{
	uint32_t size;		//in the form of n, where 2^n is the width and height of the image
	uint32_t offset;	//in the form of x, where imgHeader pos + x = file offset to raw image data
} imgDataPtr;
