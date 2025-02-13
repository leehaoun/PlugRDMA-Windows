// pch.h: 미리 컴파일된 헤더 파일입니다.
// 아래 나열된 파일은 한 번만 컴파일되었으며, 향후 빌드에 대한 빌드 성능을 향상합니다.
// 코드 컴파일 및 여러 코드 검색 기능을 포함하여 IntelliSense 성능에도 영향을 미칩니다.
// 그러나 여기에 나열된 파일은 빌드 간 업데이트되는 경우 모두 다시 컴파일됩니다.
// 여기에 자주 업데이트할 파일을 추가하지 마세요. 그러면 성능이 저하됩니다.

#ifndef PCH_H
#define PCH_H

// 여기에 미리 컴파일하려는 헤더 추가
#include "framework.h"
typedef unsigned char byte;

//3D


#include <time.h>
#include <math.h>
#include <basetsd.h>

#include "windows.h"

#define BLOCK_SIZE 1000
#define MAX_SAME_FRAME_COUNT 10000

typedef unsigned long       DWORD;
typedef int                 BOOL;
typedef unsigned char       BYTE;
typedef unsigned short      WORD;
typedef float               FLOAT;

typedef BYTE* LPBYTE;

class CCSize
{
public:
	CCSize() {}
	CCSize(int _x, int _y)
	{
		cx = _x;
		cy = _y;
	}
	int cx;
	int cy;
};
class CCPoint
{
public:
	CCPoint() {}
	CCPoint(int _x, int _y)
	{
		x = _x;
		y = _y;
	}
	int x;
	int y;
};

class CCRect
{
public:
	CCRect() {}
	CCRect(int _x, int _y, int _cx, int _cy)
	{
		x = _x;
		y = _y;
		cy = _cy;
		cx = _cx;
	}
	int x;
	int y;
	int cx;
	int cy;
};

#endif //PCH_H
