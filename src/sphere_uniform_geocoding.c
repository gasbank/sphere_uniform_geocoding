#include <math.h>
#include <limits.h>

#include "sphere_uniform_geocoding.h"

// A very short-lived native function.
//
// For very short-lived functions, it is fine to call them on the main isolate.
// They will block the Dart execution while running the native function, so
// only do this for native functions which are guaranteed to be short-lived.
FFI_PLUGIN_EXPORT intptr_t sum(intptr_t a, intptr_t b) { return a + b; }

// A longer-lived native function, which occupies the thread calling it.
//
// Do not call these kind of native functions in the main isolate. They will
// block Dart execution. This will cause dropped frames in Flutter applications.
// Instead, call these native functions on a separate isolate.
FFI_PLUGIN_EXPORT intptr_t sum_long_running(intptr_t a, intptr_t b) {
    // Simulate work.
#if _WIN32
    Sleep(5000);
#else
    usleep(5000 * 1000);
#endif
    return a + b;
}

#define NELEMS(x)  (sizeof(x) / sizeof((x)[0]))

#define Deg2Rad (0.017453292)
#ifndef M_PI
#    define M_PI (3.14159265358979323846)
#endif

#define GroupCount (20)

//void calculate_wh(void) {
//    Hh = 2 / sqrt(10 + 2 * sqrt_5);
//    Wh = Hh * (1 + sqrt_5) / 2;
//}

typedef enum {
    AxisOrientation_CCW,
    AxisOrientation_CW,
} AxisOrientation;

typedef enum {
    EdgeNeighbor_O,
    EdgeNeighbor_A,
    EdgeNeighbor_B,
} EdgeNeighbor;

typedef enum {
    EdgeNeighborOrigin_O,
    EdgeNeighborOrigin_A,
    EdgeNeighborOrigin_B,
    EdgeNeighborOrigin_Op,
    EdgeNeighborOrigin_Ap,
    EdgeNeighborOrigin_Bp,
} EdgeNeighborOrigin;

typedef enum {
    Parallelogram_Bottom,
    Parallelogram_Top,
    Parallelogram_Error,
} Parallelogram;

typedef enum {
    ErrorCode_None,
    ErrorCode_LogicError_NoIntersection = -1,
    ErrorCode_NullPtr = -2,
    ErrorCode_Argument_NullPtr = -3,
    ErrorCode_ArgumentOutOfRangeException = -4,
} ErrorCode;

typedef struct {
    int segGroupIndex;
    EdgeNeighbor edgeNeighbor;
    EdgeNeighborOrigin edgeNeighborOrigin;
    AxisOrientation axisOrientation;
} NeighborInfo;

typedef struct {
    int a;
    int b;
    Parallelogram t;
} AbtCoords;

typedef struct {
    int segGroup;
    AbtCoords abt;
} SegGroupAndAbt;

const int VertIndexPerFaces[20][3] = {
        {0, 1,  7}, // Face 0
        {0, 4,  1}, // Face 1
        {0, 7,  9}, // Face 2
        {0, 8,  4}, // Face 3
        {0, 9,  8}, // Face 4
        {1, 11, 10}, // Face 5
        {1, 10, 7}, // Face 6
        {1, 4,  11}, // Face 7
        {2, 3,  6}, // Face 8
        {2, 5,  3}, // Face 9
        {2, 6,  10}, // Face 10
        {2, 10, 11}, // Face 11
        {2, 11, 5}, // Face 12
        {3, 5,  8}, // Face 13
        {3, 8,  9}, // Face 14
        {3, 9,  6}, // Face 15
        {4, 5,  11}, // Face 16
        {4, 8,  5}, // Face 17
        {6, 7,  10}, // Face 18
        {6, 9,  7}, // Face 19
};

const Vector3 Vertices[] = {
        {0,          -0.5257311, -0.8506508},
        {0,          0.5257311,  -0.8506508},
        {0,          0.5257311,  0.8506508},
        {0,          -0.5257311, 0.8506508},
        {-0.8506508, 0,          -0.5257311},
        {-0.8506508, 0,          0.5257311},
        {0.8506508,  0,          0.5257311},
        {0.8506508,  0,          -0.5257311},
        {-0.5257311, -0.8506508, 0},
        {0.5257311,  -0.8506508, 0},
        {0.5257311,  0.8506508,  0},
        {-0.5257311, 0.8506508,  0},
};

const Vector3 SegmentGroupTriList[20][3] = {
        {
                {-4.670862E-07, -0.525732,     -0.850651},
                {-4.670862E-07, 0.525732,      -0.850651},
                {0.8506518,    0,             -0.5257308},
        },
        {
                {4.670862E-07,  -0.525732,     -0.850651},
                {-0.8506518,    0,             -0.5257308},
                {4.670862E-07, 0.525732,      -0.850651},
        },
        {
                {-7.557613E-07, -0.5257313,    -0.8506515},
                {0.8506515,     7.557613E-07,  -0.5257313},
                {0.5257313,    -0.8506515,    7.557613E-07},
        },
        {
                {7.557613E-07,  -0.5257313,    -0.8506515},
                {-0.5257313,    -0.8506515,    7.557613E-07},
                {-0.8506515,   7.557613E-07,  -0.5257313},
        },
        {
                {0,             -0.5257308,    -0.8506518},
                {0.525732,      -0.850651,     4.670862E-07},
                {-0.525732,    -0.850651,     4.670862E-07},
        },
        {
                {0,             0.5257308,     -0.8506518},
                {-0.525732,     0.850651,      4.670862E-07},
                {0.525732,     0.850651,      4.670862E-07},
        },
        {
                {-7.557613E-07, 0.5257313,     -0.8506515},
                {0.5257313,     0.8506515,     7.557613E-07},
                {0.8506515,    -7.557613E-07, -0.5257313},
        },
        {
                {7.557613E-07,  0.5257313,     -0.8506515},
                {-0.8506515,    -7.557613E-07, -0.5257313},
                {-0.5257313,   0.8506515,     7.557613E-07},
        },
        {
                {-4.670862E-07, 0.525732,      0.850651},
                {-4.670862E-07, -0.525732,     0.850651},
                {0.8506518,    0,             0.5257308},
        },
        {
                {4.670862E-07,  0.525732,      0.850651},
                {-0.8506518,    0,             0.5257308},
                {4.670862E-07, -0.525732,     0.850651},
        },
        {
                {-7.557613E-07, 0.5257313,     0.8506515},
                {0.8506515,     -7.557613E-07, 0.5257313},
                {0.5257313,    0.8506515,     -7.557613E-07},
        },
        {
                {0,             0.5257308,     0.8506518},
                {0.525732,      0.850651,      -4.670862E-07},
                {-0.525732,    0.850651,      -4.670862E-07},
        },
        {
                {7.557613E-07,  0.5257313,     0.8506515},
                {-0.5257313,    0.8506515,     -7.557613E-07},
                {-0.8506515,   -7.557613E-07, 0.5257313},
        },
        {
                {7.557613E-07,  -0.5257313,    0.8506515},
                {-0.8506515,    7.557613E-07,  0.5257313},
                {-0.5257313,   -0.8506515,    -7.557613E-07},
        },
        {
                {0,             -0.5257308,    0.8506518},
                {-0.525732,     -0.850651,     -4.670862E-07},
                {0.525732,     -0.850651,     -4.670862E-07},
        },
        {
                {-7.557613E-07, -0.5257313,    0.8506515},
                {0.5257313,     -0.8506515,    -7.557613E-07},
                {0.8506515,    7.557613E-07,  0.5257313},
        },
        {
                {-0.850651,     -4.670862E-07, -0.525732},
                {-0.850651,     -4.670862E-07, 0.525732},
                {-0.5257308,   0.8506518,     0},
        },
        {
                {-0.850651,     4.670862E-07,  -0.525732},
                {-0.5257308,    -0.8506518,    0},
                {-0.850651,    4.670862E-07,  0.525732},
        },
        {
                {0.850651,      -4.670862E-07, 0.525732},
                {0.850651,      -4.670862E-07, -0.525732},
                {0.5257308,    0.8506518,     0},
        },
        {
                {0.850651,      4.670862E-07,  0.525732},
                {0.5257308,     -0.8506518,    0},
                {0.850651,     4.670862E-07,  -0.525732},
        },
};

const AxisOrientation FaceAxisOrientationList[20] = {
        AxisOrientation_CW,
        AxisOrientation_CW,
        AxisOrientation_CW,
        AxisOrientation_CW,
        AxisOrientation_CW,
        AxisOrientation_CW,
        AxisOrientation_CW,
        AxisOrientation_CW,
        AxisOrientation_CW,
        AxisOrientation_CW,
        AxisOrientation_CW,
        AxisOrientation_CW,
        AxisOrientation_CW,
        AxisOrientation_CW,
        AxisOrientation_CW,
        AxisOrientation_CW,
        AxisOrientation_CW,
        AxisOrientation_CW,
        AxisOrientation_CW,
        AxisOrientation_CW,
};

const NeighborInfo NeighborFaceInfoList[20][3] = {
        {
                {6,  EdgeNeighbor_O, EdgeNeighborOrigin_A,  AxisOrientation_CW},
                {2,  EdgeNeighbor_A, EdgeNeighborOrigin_O,  AxisOrientation_CW},
                {1,  EdgeNeighbor_B, EdgeNeighborOrigin_O,  AxisOrientation_CW},
        },
        {
                {7,  EdgeNeighbor_O, EdgeNeighborOrigin_B,  AxisOrientation_CW},
                {0,  EdgeNeighbor_A, EdgeNeighborOrigin_O,  AxisOrientation_CW},
                {3,  EdgeNeighbor_B, EdgeNeighborOrigin_O,  AxisOrientation_CW},
        },
        {
                {19, EdgeNeighbor_O, EdgeNeighborOrigin_Op, AxisOrientation_CW},
                {4,  EdgeNeighbor_A, EdgeNeighborOrigin_O,  AxisOrientation_CW},
                {0,  EdgeNeighbor_B, EdgeNeighborOrigin_O,  AxisOrientation_CW},
        },
        {
                {17, EdgeNeighbor_O, EdgeNeighborOrigin_B,  AxisOrientation_CW},
                {1,  EdgeNeighbor_A, EdgeNeighborOrigin_O,  AxisOrientation_CW},
                {4,  EdgeNeighbor_B, EdgeNeighborOrigin_O,  AxisOrientation_CW},
        },
        {
                {14, EdgeNeighbor_O, EdgeNeighborOrigin_Op, AxisOrientation_CW},
                {3,  EdgeNeighbor_A, EdgeNeighborOrigin_O,  AxisOrientation_CW},
                {2,  EdgeNeighbor_B, EdgeNeighborOrigin_O,  AxisOrientation_CW},
        },
        {
                {11, EdgeNeighbor_O, EdgeNeighborOrigin_Op, AxisOrientation_CW},
                {6,  EdgeNeighbor_A, EdgeNeighborOrigin_O,  AxisOrientation_CW},
                {7,  EdgeNeighbor_B, EdgeNeighborOrigin_O,  AxisOrientation_CW},
        },
        {
                {18, EdgeNeighbor_O, EdgeNeighborOrigin_Op, AxisOrientation_CW},
                {0,  EdgeNeighbor_A, EdgeNeighborOrigin_Ap, AxisOrientation_CW},
                {5,  EdgeNeighbor_B, EdgeNeighborOrigin_O,  AxisOrientation_CW},
        },
        {
                {16, EdgeNeighbor_O, EdgeNeighborOrigin_A,  AxisOrientation_CW},
                {5,  EdgeNeighbor_A, EdgeNeighborOrigin_O,  AxisOrientation_CW},
                {1,  EdgeNeighbor_B, EdgeNeighborOrigin_Bp, AxisOrientation_CW},
        },
        {
                {15, EdgeNeighbor_O, EdgeNeighborOrigin_A,  AxisOrientation_CW},
                {10, EdgeNeighbor_A, EdgeNeighborOrigin_O,  AxisOrientation_CW},
                {9,  EdgeNeighbor_B, EdgeNeighborOrigin_O,  AxisOrientation_CW},
        },
        {
                {13, EdgeNeighbor_O, EdgeNeighborOrigin_B,  AxisOrientation_CW},
                {8,  EdgeNeighbor_A, EdgeNeighborOrigin_O,  AxisOrientation_CW},
                {12, EdgeNeighbor_B, EdgeNeighborOrigin_O,  AxisOrientation_CW},
        },
        {
                {18, EdgeNeighbor_O, EdgeNeighborOrigin_A,  AxisOrientation_CW},
                {11, EdgeNeighbor_A, EdgeNeighborOrigin_O,  AxisOrientation_CW},
                {8,  EdgeNeighbor_B, EdgeNeighborOrigin_O,  AxisOrientation_CW},
        },
        {
                {5,  EdgeNeighbor_O, EdgeNeighborOrigin_Op, AxisOrientation_CW},
                {12, EdgeNeighbor_A, EdgeNeighborOrigin_O,  AxisOrientation_CW},
                {10, EdgeNeighbor_B, EdgeNeighborOrigin_O,  AxisOrientation_CW},
        },
        {
                {16, EdgeNeighbor_O, EdgeNeighborOrigin_Op, AxisOrientation_CW},
                {9,  EdgeNeighbor_A, EdgeNeighborOrigin_O,  AxisOrientation_CW},
                {11, EdgeNeighbor_B, EdgeNeighborOrigin_O,  AxisOrientation_CW},
        },
        {
                {17, EdgeNeighbor_O, EdgeNeighborOrigin_Op, AxisOrientation_CW},
                {14, EdgeNeighbor_A, EdgeNeighborOrigin_O,  AxisOrientation_CW},
                {9,  EdgeNeighbor_B, EdgeNeighborOrigin_Bp, AxisOrientation_CW},
        },
        {
                {4,  EdgeNeighbor_O, EdgeNeighborOrigin_Op, AxisOrientation_CW},
                {15, EdgeNeighbor_A, EdgeNeighborOrigin_O,  AxisOrientation_CW},
                {13, EdgeNeighbor_B, EdgeNeighborOrigin_O,  AxisOrientation_CW},
        },
        {
                {19, EdgeNeighbor_O, EdgeNeighborOrigin_B,  AxisOrientation_CW},
                {8,  EdgeNeighbor_A, EdgeNeighborOrigin_Ap, AxisOrientation_CW},
                {14, EdgeNeighbor_B, EdgeNeighborOrigin_O,  AxisOrientation_CW},
        },
        {
                {12, EdgeNeighbor_O, EdgeNeighborOrigin_Op, AxisOrientation_CW},
                {7,  EdgeNeighbor_A, EdgeNeighborOrigin_Ap, AxisOrientation_CW},
                {17, EdgeNeighbor_B, EdgeNeighborOrigin_O,  AxisOrientation_CW},
        },
        {
                {13, EdgeNeighbor_O, EdgeNeighborOrigin_Op, AxisOrientation_CW},
                {16, EdgeNeighbor_A, EdgeNeighborOrigin_O,  AxisOrientation_CW},
                {3,  EdgeNeighbor_B, EdgeNeighborOrigin_Bp, AxisOrientation_CW},
        },
        {
                {6,  EdgeNeighbor_O, EdgeNeighborOrigin_Op, AxisOrientation_CW},
                {10, EdgeNeighbor_A, EdgeNeighborOrigin_Ap, AxisOrientation_CW},
                {19, EdgeNeighbor_B, EdgeNeighborOrigin_O,  AxisOrientation_CW},
        },
        {
                {2,  EdgeNeighbor_O, EdgeNeighborOrigin_Op, AxisOrientation_CW},
                {18, EdgeNeighbor_A, EdgeNeighborOrigin_O,  AxisOrientation_CW},
                {15, EdgeNeighbor_B, EdgeNeighborOrigin_Bp, AxisOrientation_CW},
        },
};

static Vector3 CalculateUnitSpherePosition(double lat, double lng) {
    double cLat = cos(lat);
    double sLat = sin(lat);

    double cLng = cos(lng);
    double sLng = sin(lng);

    return (Vector3) {.x = cLng * cLat, .y = sLat, .z = sLng * cLat};
}

static Vector3 NegateVector3(Vector3 v) {
    return (Vector3) {.x = -v.x, .y = -v.y, .z = -v.z};
}

static Vector3 AddVector3(Vector3 a, Vector3 b) {
    return (Vector3) {.x = a.x + b.x, .y = a.y + b.y, .z = a.z + b.z};
}

static Vector3 DiffVector3(Vector3 a, Vector3 b) {
    return (Vector3) {.x = a.x - b.x, .y = a.y - b.y, .z = a.z - b.z};
}

static double Dot(Vector3 v1, Vector3 v2) {
    return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
}

static Vector3 Cross(Vector3 v1, Vector3 v2) {
    return (Vector3) {
            v1.y * v2.z - v1.z * v2.y,
            v1.z * v2.x - v1.x * v2.z,
            v1.x * v2.y - v1.y * v2.x,
    };
}

static Vector3 GetTrilinearCoordinateOfTheHit(double t, Vector3 rayOrigin, Vector3 rayDirection) {
    return (Vector3) {
            .x = rayDirection.x * t + rayOrigin.x,
            .y = rayDirection.y * t + rayOrigin.y,
            .z = rayDirection.z * t + rayOrigin.z,
    };
}

static double SqrMagnitude(Vector3 v) {
    return v.x * v.x + v.y * v.y + v.z * v.z;
}

static double Magnitude(Vector3 v) {
    return sqrt(SqrMagnitude(v));
}

static Vector3 ScalarMultiplyVector(double s, Vector3 v) {
    return (Vector3) {.x = s * v.x, .y = s * v.y, .z = s * v.z};
}

#define Epsilon (0.000001)

static ErrorCode GetTimeAndUvCoord(Vector3 *output, Vector3 rayOrigin, Vector3 rayDirection, const Vector3 *vert0,
                                   const Vector3 *vert1, const Vector3 *vert2) {
    if (output == NULL) {
        return ErrorCode_NullPtr;
    }

    Vector3 edge1 = DiffVector3(*vert1, *vert0);
    Vector3 edge2 = DiffVector3(*vert2, *vert0);

    Vector3 pVec = Cross(rayDirection, edge2);

    double det = Dot(edge1, pVec);

    if (det > -Epsilon && det < Epsilon) {
        return ErrorCode_NullPtr;
    }

    double invDet = 1.0 / det;

    Vector3 tVec = DiffVector3(rayOrigin, *vert0);

    double u = Dot(tVec, pVec) * invDet;

    if (u < 0 || u > 1) {
        return ErrorCode_NullPtr;
    }

    Vector3 qVec = Cross(tVec, edge1);

    double v = Dot(rayDirection, qVec) * invDet;

    if (v < 0 || u + v > 1) {
        return ErrorCode_NullPtr;
    }

    double t = Dot(edge2, qVec) * invDet;

    // ray 반대 방향으로 만나거나, 최대 길이를 지나쳐서 만나거나 하는 건 안만나는 걸로 친다.
    if (t < 0 || t > 1) {
        return ErrorCode_NullPtr;
    }

    output->x = t;
    output->y = u;
    output->z = v;
    return ErrorCode_None;
}

static AbtCoords
CalculateAbCoords(int n, const Vector3 *ip0, const Vector3 *ip1, const Vector3 *ip2, Vector3 intersect) {
    Vector3 p = DiffVector3(intersect, *ip0);
    Vector3 p01 = DiffVector3(*ip1, *ip0);
    Vector3 p02 = DiffVector3(*ip2, *ip0);

    double a = Dot(p, p01) / SqrMagnitude(p01);
    double b = Dot(p, p02) / SqrMagnitude(p02);

    double tanDelta = Magnitude(Cross(p01, p02)) / Dot(p01, p02);

    double ap = a - Magnitude(DiffVector3(p, ScalarMultiplyVector(a, p01))) / (tanDelta * Magnitude(p01));
    double bp = b - Magnitude(DiffVector3(p, ScalarMultiplyVector(b, p02))) / (tanDelta * Magnitude(p02));

    double api, bpi;
    double apf = modf(ap * n, &api);
    double bpf = modf(bp * n, &bpi);

    //ap * SubdivisionCount
    return (AbtCoords) {.a = (int) api, .b = (int) bpi, .t = apf + bpf > 1};
}

// n(분할 횟수), AB 좌표, top여부 세 개를 조합해 세그먼트 그룹 내 인덱스를 계산하여 반환한다.
static int ConvertToLocalSegmentIndex(int n, int a, int b, Parallelogram top) {
    if (n <= 0) {
        //throw new ArgumentOutOfRangeException(nameof(n));
        return ErrorCode_ArgumentOutOfRangeException;
    }

    if (a + b >= n) {
        //throw new ArgumentOutOfRangeException($"{nameof(a)} + {nameof(b)}");
        return ErrorCode_ArgumentOutOfRangeException;
    }

    if (a + b == n - 1 && top) {
        // 오른쪽 가장자리 변에 맞닿은 세그먼트는 top일 수 없다.
        //throw new ArgumentOutOfRangeException($"{nameof(a)} + {nameof(b)} + {nameof(top)}");
        return ErrorCode_ArgumentOutOfRangeException;
    }

    const int parallelogramIndex = b * n - (b - 1) * b / 2 + a;
    return parallelogramIndex * 2 - b + (top ? 1 : 0);
}

static int ConvertToLocalSegmentIndex2(int n, AbtCoords abtCoords) {
    return ConvertToLocalSegmentIndex(n, abtCoords.a, abtCoords.b, abtCoords.t);
}

static int CalculateSegmentCountPerGroup(int n) {
    return n * n;
}

FFI_PLUGIN_EXPORT int ConvertToSegmentIndex2(const int n, int segmentGroupIndex, int localSegmentIndex) {
    uint32_t segmentCountPerGroup = CalculateSegmentCountPerGroup(n);
    if (segmentGroupIndex < 0 || segmentGroupIndex >= GroupCount) {
        return -1;
    }

    if (localSegmentIndex < 0 || localSegmentIndex >= segmentCountPerGroup) {
        return -1;
    }

    return (int) ((int64_t) segmentCountPerGroup * segmentGroupIndex + localSegmentIndex);
}

// 세그먼트 그룹 인덱스, n(분할 횟수), AB 좌표, top여부 네 개를 조합 해 전역 세그먼트 인덱스를 계산하여 반환한다.
static int ConvertToSegmentIndex(int segmentGroupIndex, int n, int a, int b, Parallelogram top) {
    const int localSegmentIndex = ConvertToLocalSegmentIndex(n, a, b, top);

    return ConvertToSegmentIndex2(n, segmentGroupIndex, localSegmentIndex);
}

FFI_PLUGIN_EXPORT int CalculateSegmentIndexFromLatLng(int n, double userPosLat, double userPosLng) {
    Vector3 userPosFromLatLng = ScalarMultiplyVector(2, CalculateUnitSpherePosition(userPosLat, userPosLng));

    int segGroupIndex = -1;
    Vector3 intersect = {0, 0, 0};
    for (int index = 0; index < NELEMS(SegmentGroupTriList); index++) {
        const Vector3 *segTriList = SegmentGroupTriList[index];
        Vector3 intersectTuv;
        if (GetTimeAndUvCoord(&intersectTuv, userPosFromLatLng, NegateVector3(userPosFromLatLng), segTriList + 0,
                              segTriList + 1, segTriList + 2) != ErrorCode_NullPtr) {
            segGroupIndex = index;
            intersect =
                    GetTrilinearCoordinateOfTheHit(intersectTuv.x, userPosFromLatLng,
                                                   NegateVector3(userPosFromLatLng));
            break;
        }
    }

    if (segGroupIndex < 0 || segGroupIndex >= 20) {
        return ErrorCode_LogicError_NoIntersection;
    }

    const Vector3 *triList = SegmentGroupTriList[segGroupIndex];

    AbtCoords abtCoords = CalculateAbCoords(n, triList + 0, triList + 1, triList + 2, intersect);

    return ConvertToSegmentIndex(segGroupIndex, n, abtCoords.a, abtCoords.b, abtCoords.t);
}

// AB 좌표의 B 좌표로 시작되는 세그먼트 서브 인덱스의 시작값을 계산한다.
static int CalculateLocalSegmentIndexForB(int n, int b) {
    if (n <= 0) {
        //throw new IndexOutOfRangeException(nameof(n));
        return ErrorCode_ArgumentOutOfRangeException;
    }

    return ConvertToLocalSegmentIndex(n, 0, b, Parallelogram_Bottom);
}

// 세그먼트 서브 인덱스가 주어졌을 때, B 좌표를 이진 탐색 방법으로 찾아낸다.
// 단, 찾아낸 B 좌표는 b0 ~ b1 범위에 있다고 가정한다.
static int SearchForB(int n, int b0, int b1, int localSegmentIndex) {
    if (n <= 0) {
        //throw new IndexOutOfRangeException(nameof(n));
        return ErrorCode_ArgumentOutOfRangeException;
    }

    if (b0 < 0) {
        //throw new IndexOutOfRangeException(nameof(b0));
        return ErrorCode_ArgumentOutOfRangeException;
    }

    if (b1 >= n) {
        //throw new IndexOutOfRangeException(nameof(b1));
        return ErrorCode_ArgumentOutOfRangeException;
    }

    if (b0 > b1) {
        //throw new IndexOutOfRangeException($"{nameof(b0)}, {nameof(b1)}");
        return ErrorCode_ArgumentOutOfRangeException;
    }

    int localIndex0 = CalculateLocalSegmentIndexForB(n, b0);
    int localIndex1 = CalculateLocalSegmentIndexForB(n, b1);

    if (localIndex0 > localSegmentIndex || localIndex1 < localSegmentIndex) {
        //throw new IndexOutOfRangeException(nameof(localSegmentIndex));
        return ErrorCode_ArgumentOutOfRangeException;
    }

    if (localIndex0 == localSegmentIndex) {
        return b0;
    }

    if (localIndex1 == localSegmentIndex) {
        return b1;
    }

    while (b1 - b0 > 1) {
        int bMid = (b0 + b1) / 2;
        int v = CalculateLocalSegmentIndexForB(n, bMid) - localSegmentIndex;
        if (v < 0) {
            b0 = bMid;
            continue;
        }
        if (v > 0) {
            b1 = bMid;
            continue;
        }

        return bMid;
    }

    return b0;
}

FFI_PLUGIN_EXPORT SegGroupAndLocalSegIndex
SplitSegIndexToSegGroupAndLocalSegmentIndex(const int n, const int segmentIndex) {
    if (n < 1) {
        return (SegGroupAndLocalSegIndex) {.segGroup = -1, .localSegIndex = -1};
    }

    uint32_t segmentCountPerGroup = CalculateSegmentCountPerGroup(n);

    int64_t unsignedMaxSegCount = (int64_t) segmentCountPerGroup * GroupCount;

    if (unsignedMaxSegCount > (int64_t) UINT_MAX + 1) {
        return (SegGroupAndLocalSegIndex) {.segGroup = -1, .localSegIndex = -1};
    }

    uint32_t unsignedSegmentIndex = (uint32_t) segmentIndex;
    if (unsignedSegmentIndex >= unsignedMaxSegCount) {
        return (SegGroupAndLocalSegIndex) {.segGroup = -1, .localSegIndex = -1};
    }

    int quotient = (int) (unsignedSegmentIndex / segmentCountPerGroup);
    int remainder = (int) (unsignedSegmentIndex % segmentCountPerGroup);

    if (quotient < 0 || quotient >= GroupCount) {
        return (SegGroupAndLocalSegIndex) {.segGroup = -1, .localSegIndex = -1};
    }

    if (remainder < 0 || remainder >= segmentCountPerGroup) {
        return (SegGroupAndLocalSegIndex) {.segGroup = -1, .localSegIndex = -1};
    }

    // (segmentGroupIndex, localSegIndex)
    return (SegGroupAndLocalSegIndex) {.segGroup = quotient, .localSegIndex = remainder};
}

static AbtCoords SplitLocalSegmentIndexToAbt(int n, int localSegmentIndex) {
    if (n <= 0) {
        return (AbtCoords) {.a = INT32_MIN, .b = INT32_MIN, .t = Parallelogram_Error};
    }

    int b = SearchForB(n, 0, n - 1, localSegmentIndex);
    int a = (localSegmentIndex - CalculateLocalSegmentIndexForB(n, b)) / 2;
    Parallelogram t = !((b % 2 == 0 && localSegmentIndex % 2 == 0) || (b % 2 == 1 && localSegmentIndex % 2 == 1))
                      ? Parallelogram_Top
                      : Parallelogram_Bottom;
    return (AbtCoords) {.a = a, .b = b, .t = t};
}

static SegGroupAndAbt SplitSegIndexToSegGroupAndAbt(const int n, const int segmentIndex) {
    SegGroupAndLocalSegIndex segGroupAndLocalSegIndex = SplitSegIndexToSegGroupAndLocalSegmentIndex(n, segmentIndex);
    AbtCoords abt = SplitLocalSegmentIndexToAbt(n, segGroupAndLocalSegIndex.localSegIndex);
    return (SegGroupAndAbt) {.segGroup = segGroupAndLocalSegIndex.segGroup, .abt = abt};
}

static Vector3 NormalizeVector3(Vector3 v) {
    double m = Magnitude(v);
    return ScalarMultiplyVector(1.0 / m, v);
}

// Seg Index의 중심 좌표를 계산해서 반환
FFI_PLUGIN_EXPORT Vector3 CalculateSegmentCenter(const int n, const int segmentIndex) {
    const SegGroupAndAbt segGroupAndAbt = SplitSegIndexToSegGroupAndAbt(n, segmentIndex);
    //Vector3 segGroupVerts[] = { VertIndexPerFaces[segGroupIndex].Select(e => Vertices[e]).ToArray();
    const Vector3 segGroupVerts[] = {
            Vertices[VertIndexPerFaces[segGroupAndAbt.segGroup][0]],
            Vertices[VertIndexPerFaces[segGroupAndAbt.segGroup][1]],
            Vertices[VertIndexPerFaces[segGroupAndAbt.segGroup][2]],
    };
    const Vector3 axisA = ScalarMultiplyVector(1.0 / n, DiffVector3(segGroupVerts[1], segGroupVerts[0]));
    const Vector3 axisB = ScalarMultiplyVector(1.0 / n, DiffVector3(segGroupVerts[2], segGroupVerts[0]));

    //Vector3 parallelogramCorner = segGroupVerts[0] + ScalarMultiplyVector(ab.x, axisA) + ScalarMultiplyVector(ab.y * axisB);
    Vector3 parallelogramCorner = {0, 0, 0};
    parallelogramCorner = AddVector3(parallelogramCorner, segGroupVerts[0]);
    parallelogramCorner = AddVector3(parallelogramCorner, ScalarMultiplyVector(segGroupAndAbt.abt.a, axisA));
    parallelogramCorner = AddVector3(parallelogramCorner, ScalarMultiplyVector(segGroupAndAbt.abt.b, axisB));
    //Vector3 offset = axisA + axisB;
    const Vector3 offset = AddVector3(axisA, axisB);
    return NormalizeVector3(AddVector3(parallelogramCorner,
                                       ScalarMultiplyVector(
                                               1.0 / 3 * (segGroupAndAbt.abt.t == Parallelogram_Top ? 2 : 1), offset)));
}

// https://stackoverflow.com/questions/1628386/normalise-orientation-between-0-and-360
// Normalizes any number to an arbitrary range
// by assuming the range wraps around when going below min or above max
static double Normalize(double value, double start, double end) {
    double width = end - start; //
    double offsetValue = value - start; // value relative to 0

    return (offsetValue - (floor(offsetValue / width) * width)) + start;
    // + start to reset back to start of original range
}

static double Sign(double v) {
    return (v > 0) - (v < 0);
}

static double Clamp(double v, double lo, double hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static double Angle(Vector3 from, Vector3 to) {
    const double num = sqrt(SqrMagnitude(from) * SqrMagnitude(to));
    return num < 1.0000000036274937E-15 ? 0.0 : acos(Clamp(Dot(from, to) / num, -1, 1)) * 57.29578;
}

// 임의의 지점 p의 위도, 경도를 계산하여 라디안으로 반환한다.
// 위도는 -pi/2 ~ +pi/2 범위
// 경도는 -pi ~ pi 범위다.
static GpsCoords CalculateLatLng(Vector3 p) {
    const Vector3 pNormalized = NormalizeVector3(p);

    const double lng = Normalize(atan2(pNormalized.z, pNormalized.x), -M_PI, M_PI);

    const Vector3 lngVec = {cos(lng), 0, sin(lng)};

    const double lat = Normalize(Sign(pNormalized.y) * Angle(lngVec, pNormalized) * Deg2Rad, -M_PI / 2,
                                 M_PI / 2);
    return (GpsCoords) {lat, lng};
}

// Seg Index의 중심 좌표의 위도를 계산해서 반환
FFI_PLUGIN_EXPORT double CalculateSegmentCenterLat(int n, int segmentIndex) {
    return CalculateLatLng(CalculateSegmentCenter(n, segmentIndex)).lat;
}

// Seg Index의 중심 좌표의 위도를 계산해서 반환
FFI_PLUGIN_EXPORT double CalculateSegmentCenterLng(int n, int segmentIndex) {
    return CalculateLatLng(CalculateSegmentCenter(n, segmentIndex)).lng;
}

// Seg Index의 중심 좌표의 X축을 계산해서 반환
FFI_PLUGIN_EXPORT double CalculateSegmentCenterX(int n, int segmentIndex) {
    return CalculateSegmentCenter(n, segmentIndex).x;
}

// Seg Index의 중심 좌표의 Y축을 계산해서 반환
FFI_PLUGIN_EXPORT double CalculateSegmentCenterY(int n, int segmentIndex) {
    return CalculateSegmentCenter(n, segmentIndex).y;
}

// Seg Index의 중심 좌표의 Y축을 계산해서 반환
FFI_PLUGIN_EXPORT double CalculateSegmentCenterZ(int n, int segmentIndex) {
    return CalculateSegmentCenter(n, segmentIndex).z;
}

// Seg Index의 세 정점 위치를 계산해서 반환
static void CalculateSegmentCorners(Vector3 *out, int n, int segmentIndex, int normalize) {
    const SegGroupAndAbt segGroupAndAbt = SplitSegIndexToSegGroupAndAbt(n, segmentIndex);
    const Vector3 segGroupVerts[] = {
            Vertices[VertIndexPerFaces[segGroupAndAbt.segGroup][0]],
            Vertices[VertIndexPerFaces[segGroupAndAbt.segGroup][1]],
            Vertices[VertIndexPerFaces[segGroupAndAbt.segGroup][2]],
    };
    const Vector3 axisA = ScalarMultiplyVector(1.0 / n, DiffVector3(segGroupVerts[1], segGroupVerts[0]));
    const Vector3 axisB = ScalarMultiplyVector(1.0 / n, DiffVector3(segGroupVerts[2], segGroupVerts[0]));

    const Vector3 parallelogramCorner = AddVector3(segGroupVerts[0],
                                                   AddVector3(ScalarMultiplyVector(segGroupAndAbt.abt.a, axisA),
                                                              ScalarMultiplyVector(segGroupAndAbt.abt.b, axisB)));

    if (segGroupAndAbt.abt.t == Parallelogram_Top) {
        out[0] = AddVector3(parallelogramCorner, AddVector3(axisA, axisB));
    } else {
        out[0] = parallelogramCorner;

    }
    out[1] = AddVector3(parallelogramCorner, axisA);
    out[2] = AddVector3(parallelogramCorner, axisB);

    if (normalize) {
        NormalizeVector3(out[0]);
        NormalizeVector3(out[1]);
        NormalizeVector3(out[2]);
    }
}

// Seg Index의 세 정점 위치를 계산해서 반환 (위도, 경도)
FFI_PLUGIN_EXPORT SegmentCornersInLatLng CalculateSegmentCornersInLatLng(int n, int segmentIndex) {
    SegmentCornersInLatLng ret = {0};
    Vector3 points[3];
    CalculateSegmentCorners(points, n, segmentIndex, 0);
    for (int i = 0; i < 3; i++) {
        ret.points[i] = CalculateLatLng(points[i]);
    }
    return ret;
}

const AbtCoords NeighborOffsetSubdivisionOne[] = {
        // 하단 행
        {0,  -1, Parallelogram_Bottom},
        {0,  -1, Parallelogram_Top},
        {1,  -1, Parallelogram_Bottom},
        // 지금 행
        {-1, 0,  Parallelogram_Bottom},
        {-1, 0,  Parallelogram_Top},
        {0,  0,  Parallelogram_Top},
        {1,  0,  Parallelogram_Bottom},
        // 상단 행
        {-1, 1,  Parallelogram_Bottom},
        {0,  1,  Parallelogram_Bottom},
};

const AbtCoords NeighborOffsetTop[] = {
        // 하단 행
        {0,  -1, Parallelogram_Top},
        {1,  -1, Parallelogram_Bottom},
        {1,  -1, Parallelogram_Top},
        // 지금 행
        {-1, 0,  Parallelogram_Top},
        {0,  0,  Parallelogram_Bottom},
        {1,  0,  Parallelogram_Bottom},
        {1,  0,  Parallelogram_Top},
        // 상단 행
        {-1, 1,  Parallelogram_Bottom},
        {-1, 1,  Parallelogram_Top},
        {0,  1,  Parallelogram_Bottom},
        {0,  1,  Parallelogram_Top},
        {1,  1,  Parallelogram_Bottom},
};

const AbtCoords NeighborOffsetBottom[] = {
        // 하단 행
        {-1, -1, Parallelogram_Top},
        {0,  -1, Parallelogram_Bottom},
        {0,  -1, Parallelogram_Top},
        {1,  -1, Parallelogram_Bottom},
        {1,  -1, Parallelogram_Top},
        // 지금 행
        {-1, 0,  Parallelogram_Bottom},
        {-1, 0,  Parallelogram_Top},
        {0,  0,  Parallelogram_Top},
        {1,  0,  Parallelogram_Bottom},
        // 상단 행
        {-1, 1,  Parallelogram_Bottom},
        {-1, 1,  Parallelogram_Top},
        {0,  1,  Parallelogram_Bottom},
};

typedef enum {
    SegmentGroupNeighbor_Inside,
    SegmentGroupNeighbor_O,
    SegmentGroupNeighbor_A,
    SegmentGroupNeighbor_B,
    SegmentGroupNeighbor_OA,
    SegmentGroupNeighbor_OB,
    SegmentGroupNeighbor_AO,
    SegmentGroupNeighbor_AB,
    SegmentGroupNeighbor_BO,
    SegmentGroupNeighbor_BA,
    SegmentGroupNeighbor_Outside,
    SegmentGroupNeighbor_Error,
} SegmentGroupNeighbor;

typedef struct {
    SegmentGroupNeighbor segGroupNeighbor;
    AbtCoords abt;
} SegmentGroupNeighborAndAbt;

typedef struct {
    SegmentGroupNeighborAndAbt segGroupNeighborAbt[12];
    int count;
} LocalNeighborSegList;

typedef enum {
    ParallelogramGroup_Bottom,
    ParallelogramGroup_Top,
    ParallelogramGroup_Outside,
    ParallelogramGroup_Error,
} ParallelogramGroup;

// ABT 좌표계가 가리키는 위치가 평행사변형의 상단인지 하단인지 판단하여 반환한다.
// 상단이면 true, 하단이면 false를 반환하며, 범위를 벗어나는 경우에는 예외를 던진다.
static ParallelogramGroup CheckBottomOrTopFromParallelogram(int n, AbtCoords abtCoords) {
    if (n < 1) {
        //throw new ArgumentOutOfRangeException(nameof(n));
        return ParallelogramGroup_Error;
    }

    if (abtCoords.a < 0 || abtCoords.a >= n || abtCoords.b < 0 || abtCoords.b >= n) {
        return ParallelogramGroup_Outside;
    }

    if (abtCoords.a + abtCoords.b < n - 1) {
        return ParallelogramGroup_Bottom;
    }

    return abtCoords.a + abtCoords.b != n - 1
           || abtCoords.t == Parallelogram_Top ? ParallelogramGroup_Top : ParallelogramGroup_Bottom;
}

// ABT 좌표가 어떤 SegmentGroupNeighbor에 속하는지를 체크해서 반환한다.
static SegmentGroupNeighbor CheckSegmentGroupNeighbor(int n, AbtCoords abtCoords) {
    switch (CheckBottomOrTopFromParallelogram(n, abtCoords)) {
        case ParallelogramGroup_Bottom:
            return SegmentGroupNeighbor_Inside;
        case ParallelogramGroup_Top:
            return SegmentGroupNeighbor_O;
        case ParallelogramGroup_Outside:
            switch (CheckBottomOrTopFromParallelogram(n, (AbtCoords) {abtCoords.a - n, abtCoords.b, abtCoords.t})) {
                case ParallelogramGroup_Bottom:
                    return SegmentGroupNeighbor_OB;
                case ParallelogramGroup_Top:
                    return SegmentGroupNeighbor_Outside;
                case ParallelogramGroup_Outside:
                    break;
                default:
                    //throw new ArgumentOutOfRangeException();
                    return SegmentGroupNeighbor_Error;
            }

            switch (CheckBottomOrTopFromParallelogram(n, (AbtCoords) {abtCoords.a + n, abtCoords.b, abtCoords.t})) {
                case ParallelogramGroup_Bottom:
                    return SegmentGroupNeighbor_AB;
                case ParallelogramGroup_Top:
                    return SegmentGroupNeighbor_A;
                case ParallelogramGroup_Outside:
                    break;
                default:
                    //throw new ArgumentOutOfRangeException();
                    return SegmentGroupNeighbor_Error;
            }

            switch (CheckBottomOrTopFromParallelogram(n, (AbtCoords) {abtCoords.a, abtCoords.b - n, abtCoords.t})) {
                case ParallelogramGroup_Bottom:
                    return SegmentGroupNeighbor_OA;
                case ParallelogramGroup_Top:
                    return SegmentGroupNeighbor_Outside;
                case ParallelogramGroup_Outside:
                    break;
                default:
                    //throw new ArgumentOutOfRangeException();
                    return SegmentGroupNeighbor_Error;
            }

            switch (CheckBottomOrTopFromParallelogram(n, (AbtCoords) {abtCoords.a, abtCoords.b + n, abtCoords.t})) {
                case ParallelogramGroup_Bottom:
                    return SegmentGroupNeighbor_BA;
                case ParallelogramGroup_Top:
                    return SegmentGroupNeighbor_B;
                case ParallelogramGroup_Outside:
                    break;
                default:
                    //throw new ArgumentOutOfRangeException();
                    return SegmentGroupNeighbor_Error;
            }

            switch (CheckBottomOrTopFromParallelogram(n, (AbtCoords) {abtCoords.a + n, abtCoords.b - n, abtCoords.t})) {
                case ParallelogramGroup_Bottom:
                    return SegmentGroupNeighbor_AO;
                case ParallelogramGroup_Top:
                    return SegmentGroupNeighbor_Outside;
                case ParallelogramGroup_Outside:
                    break;
                default:
                    //throw new ArgumentOutOfRangeException();
                    return SegmentGroupNeighbor_Error;
            }

            switch (CheckBottomOrTopFromParallelogram(n, (AbtCoords) {abtCoords.a - n, abtCoords.b + n, abtCoords.t})) {
                case ParallelogramGroup_Bottom:
                    return SegmentGroupNeighbor_BO;
                case ParallelogramGroup_Top:
                    return SegmentGroupNeighbor_Outside;
                case ParallelogramGroup_Outside:
                    return SegmentGroupNeighbor_Outside;
                default:
                    //throw new ArgumentOutOfRangeException();
                    return SegmentGroupNeighbor_Error;
            }
        default:
            //throw new ArgumentOutOfRangeException();
            return SegmentGroupNeighbor_Error;
    }
}

// 하나의 세그먼트 그룹을 벗어나 인접한 세그먼트 그룹 내 세그먼트를 가리키는
// ABT 좌표를 인접한 세그먼트 그룹과 해당 세그먼트 그룹 내의 유효한 ABT 좌표로 변환하여 반환한다.
static SegmentGroupNeighborAndAbt ConvertAbtToNeighborAbt(int n, AbtCoords abtCoords) {

    if (n < 1) {
        //throw new ArgumentOutOfRangeException(nameof(n));
        return (SegmentGroupNeighborAndAbt) {
                .segGroupNeighbor = SegmentGroupNeighbor_Error, .abt = {INT32_MIN, INT32_MIN, Parallelogram_Error},
        };
    }

    const SegmentGroupNeighbor segmentGroupNeighbor = CheckSegmentGroupNeighbor(n, abtCoords);
    return (SegmentGroupNeighborAndAbt) {
            .segGroupNeighbor = segmentGroupNeighbor, .abt = abtCoords,
    };
}

static LocalNeighborSegList GetLocalSegmentIndexNeighborsAsAbtCase3Bottom(int n, AbtCoords abCoords /* t 안씀 */) {
    int skipIndex = -1;
    if (abCoords.a == 0 && abCoords.b == 0) {
        skipIndex = 0;
    } else if (abCoords.a == n - 1 && abCoords.b == 0) {
        skipIndex = 4;
    } else if (abCoords.a == 0 && abCoords.b == n - 1) {
        skipIndex = 10;
    }

    LocalNeighborSegList ret = {.count = 0};
    for (int i = 0; i < NELEMS(NeighborOffsetBottom); i++) {
        if (i == skipIndex) {
            continue;
        }
        const AbtCoords d = NeighborOffsetBottom[i];
        ret.segGroupNeighborAbt[ret.count] = ConvertAbtToNeighborAbt(n, (AbtCoords) {abCoords.a + d.a, abCoords.b + d.b,
                                                                                     d.t});
        ret.count++;
    }
    return ret;
}

static LocalNeighborSegList GetLocalSegmentIndexNeighborsAsAbt(int n, int localSegmentIndex) {

    if (n < 1) {
        //throw new ArgumentOutOfRangeException(nameof(n));
        return (LocalNeighborSegList) {.count = -1};
    }

    LocalNeighborSegList ret = {.count = 0};
    const AbtCoords abtCoords = SplitLocalSegmentIndexToAbt(n, localSegmentIndex);

    // i) 정이십면체 그대로인 상태일 때는 특수한 케이스이다. (테스트 안해봄)
    if (n == 1) {
        // n=1일 때는 top, bottom이라는 개념 없이 모두 bottom이다.
        for (int i = 0; i < NELEMS(NeighborOffsetSubdivisionOne); i++) {
            const AbtCoords d = NeighborOffsetSubdivisionOne[i];
            ret.segGroupNeighborAbt[ret.count] = ConvertAbtToNeighborAbt(n, (AbtCoords) {abtCoords.a + d.a,
                                                                                         abtCoords.b + d.b, d.t});
            ret.count++;
        }
        return ret;
    }

    // iii) 그 밖의 경우
    if (abtCoords.t == Parallelogram_Top) {
        // top인 경우
        for (int i = 0; i < NELEMS(NeighborOffsetTop); i++) {
            const AbtCoords d = NeighborOffsetTop[i];
            ret.segGroupNeighborAbt[ret.count] = ConvertAbtToNeighborAbt(n, (AbtCoords) {abtCoords.a + d.a,
                                                                                         abtCoords.b + d.b, d.t});
            ret.count++;
        }
        return ret;
    }

    // bottom인 경우에는 코너인지 아닌지에 따라서도 처리가 달라진다.
    return GetLocalSegmentIndexNeighborsAsAbtCase3Bottom(n, abtCoords);
}

static AbtCoords Swap(int a, int b, Parallelogram t, int swap) {
    return !swap ? (AbtCoords) {a, b, t} : (AbtCoords) {b, a, t};
}

static AbtCoords ConvertCoordinate(AxisOrientation baseAxisOrientation, EdgeNeighbor edgeNeighbor,
                                   EdgeNeighborOrigin edgeNeighborOrigin, AxisOrientation axisOrientation, int n,
                                   AbtCoords abtCoords) {
    const int a = abtCoords.a;
    const int b = abtCoords.b;
    const int tv = abtCoords.t == Parallelogram_Top ? 1 : 0;
    const Parallelogram tInvert = abtCoords.t == Parallelogram_Bottom ? Parallelogram_Top : Parallelogram_Bottom;
    int swap = baseAxisOrientation != axisOrientation;

    switch (edgeNeighbor) {
        case EdgeNeighbor_O:
            switch (edgeNeighborOrigin) {
                case EdgeNeighborOrigin_A:
                    return Swap(a + b + tv - n, -a + (n - 1), tInvert, swap);
                case EdgeNeighborOrigin_B:
                    return Swap(-b + (n - 1), a + b + tv - n, tInvert, swap);
                case EdgeNeighborOrigin_Op:
                    return Swap(-a + (n - 1), -b + (n - 1), tInvert, swap);
                case EdgeNeighborOrigin_O:
                case EdgeNeighborOrigin_Ap:
                case EdgeNeighborOrigin_Bp:
                default:
                    //throw new ArgumentOutOfRangeException(nameof(edgeNeighborOrigin), edgeNeighborOrigin, null);
                    return (AbtCoords) {.a = INT32_MIN, .b = INT32_MIN, .t = Parallelogram_Error};
            }
        case EdgeNeighbor_A:
            switch (edgeNeighborOrigin) {
                case EdgeNeighborOrigin_Ap:
                    return Swap(-b + (n - 1), a + b + tv, tInvert, swap);
                case EdgeNeighborOrigin_B:
                    return Swap(-a - 1, -b + (n - 1), tInvert, swap);
                case EdgeNeighborOrigin_O:
                    return Swap(a + b + tv, -a - 1, tInvert, swap);
                case EdgeNeighborOrigin_A:
                case EdgeNeighborOrigin_Bp:
                case EdgeNeighborOrigin_Op:
                default:
                    //throw new ArgumentOutOfRangeException(nameof(edgeNeighborOrigin), edgeNeighborOrigin, null);
                    return (AbtCoords) {.a = INT32_MIN, .b = INT32_MIN, .t = Parallelogram_Error};
            }
        case EdgeNeighbor_B:
            switch (edgeNeighborOrigin) {
                case EdgeNeighborOrigin_A:
                    return Swap(-a + (n - 1), -b - 1, tInvert, swap);
                case EdgeNeighborOrigin_Bp:
                    return Swap(a + b + tv, -a + (n - 1), tInvert, swap);
                case EdgeNeighborOrigin_O:
                    return Swap(-b - 1, a + b + tv, tInvert, swap);
                case EdgeNeighborOrigin_Ap:
                case EdgeNeighborOrigin_B:
                case EdgeNeighborOrigin_Op:
                default:
                    //throw new ArgumentOutOfRangeException(nameof(edgeNeighborOrigin), edgeNeighborOrigin, null);
                    return (AbtCoords) {.a = INT32_MIN, .b = INT32_MIN, .t = Parallelogram_Error};
            }
        default:
            //throw new ArgumentOutOfRangeException(nameof(edgeNeighbor), edgeNeighbor, null);
            return (AbtCoords) {.a = INT32_MIN, .b = INT32_MIN, .t = Parallelogram_Error};
    }
}

static AbtCoords
ConvertCoordinate2(AxisOrientation baseAxisOrientation, NeighborInfo neighborInfo, int n, AbtCoords abtCoords) {
    return ConvertCoordinate(baseAxisOrientation, neighborInfo.edgeNeighbor, neighborInfo.edgeNeighborOrigin,
                             neighborInfo.axisOrientation, n, abtCoords);
}

static int
ConvertCoordinateByNeighborInfo(const AxisOrientation baseAxisOrientation, const NeighborInfo neighborInfo, int n,
                                AbtCoords neighborAbt) {
    const AbtCoords convertedAbt = ConvertCoordinate(baseAxisOrientation, neighborInfo.edgeNeighbor,
                                                     neighborInfo.edgeNeighborOrigin, neighborInfo.axisOrientation, n,
                                                     neighborAbt);
    const int convertedNeighborLocalSegIndex = ConvertToLocalSegmentIndex(n, convertedAbt.a, convertedAbt.b,
                                                                          convertedAbt.t);
    const int convertedNeighborSegIndex = ConvertToSegmentIndex2(n, neighborInfo.segGroupIndex,
                                                                 convertedNeighborLocalSegIndex);
    return convertedNeighborSegIndex;
}

static NeighborInfo GetNeighbor2SegGroupIndex(const int segGroupIndex, const int n1Index, const int n2Index) {
    const int *segGroupVertIndexList = VertIndexPerFaces[segGroupIndex];
    const NeighborInfo n1Info = NeighborFaceInfoList[segGroupIndex][n1Index];
    const NeighborInfo *n2InfoList = NeighborFaceInfoList[n1Info.segGroupIndex];
    for (int i = 0; i < 3; i++) {
        const NeighborInfo *n2Info = n2InfoList + i;
        if (n2Info->segGroupIndex != segGroupIndex) {
            int contains = 0;
            for (int j = 0; j < 3; j++) {
                if (VertIndexPerFaces[n2Info->segGroupIndex][j] == segGroupVertIndexList[n2Index]) {
                    contains = 1;
                    break;
                }
            }
            if (contains == 0) {
                return *n2Info;
            }
        }
    }
    return (NeighborInfo) {.segGroupIndex=INT32_MIN};
}

static NeighborInfo GetNeighborInfoOfSegGroupIndex(const int segGroupIndex, const SegmentGroupNeighbor neighbor) {
    if (segGroupIndex < 0 || segGroupIndex >= 20) {
        // throw new ArgumentOutOfRangeException(nameof(segGroupIndex), segGroupIndex, null);
        return (NeighborInfo) {.segGroupIndex=INT32_MIN};
    }

    switch (neighbor) {
        case SegmentGroupNeighbor_Inside:
            //throw new ArgumentException(nameof(neighbor));
            return (NeighborInfo) {.segGroupIndex=INT32_MIN};
        case SegmentGroupNeighbor_O:
            return NeighborFaceInfoList[segGroupIndex][0];
        case SegmentGroupNeighbor_A:
            return NeighborFaceInfoList[segGroupIndex][1];
        case SegmentGroupNeighbor_B:
            return NeighborFaceInfoList[segGroupIndex][2];
        case SegmentGroupNeighbor_OA:
            return GetNeighbor2SegGroupIndex(segGroupIndex, 0, 1);
        case SegmentGroupNeighbor_OB:
            return GetNeighbor2SegGroupIndex(segGroupIndex, 0, 2);
        case SegmentGroupNeighbor_AO:
            return GetNeighbor2SegGroupIndex(segGroupIndex, 1, 0);
        case SegmentGroupNeighbor_AB:
            return GetNeighbor2SegGroupIndex(segGroupIndex, 1, 2);
        case SegmentGroupNeighbor_BO:
            return GetNeighbor2SegGroupIndex(segGroupIndex, 2, 0);
        case SegmentGroupNeighbor_BA:
            return GetNeighbor2SegGroupIndex(segGroupIndex, 2, 1);
        case SegmentGroupNeighbor_Outside:
        default:
            //throw new ArgumentOutOfRangeException(nameof(neighbor), neighbor, null);
            return (NeighborInfo) {.segGroupIndex=INT32_MIN};
    }
}

// 세그먼트 그룹 내에서 완전히 모든 이웃 세그먼트가 찾아지지 않고,
// 세그먼트 그룹 경계를 벗어나는 이웃이 포함되는 경우에
// 이웃 세그먼트 인덱스를 모두 반환한다.
// 여러 세그먼트 그룹에 걸쳐야하므로, 세그먼트 서브 인덱스로 조회할 수는 없다.
FFI_PLUGIN_EXPORT NeighborSegIdList GetNeighborsOfSegmentIndex(const int n, const int segmentIndex) {
    const SegGroupAndLocalSegIndex segGroupAndLocalSegIndex = SplitSegIndexToSegGroupAndLocalSegmentIndex(n,
                                                                                                          segmentIndex);
    const int segGroupIndex = segGroupAndLocalSegIndex.segGroup;
    const int localSegmentIndex = segGroupAndLocalSegIndex.localSegIndex;

    const AxisOrientation baseAxisOrientation = FaceAxisOrientationList[segGroupIndex];

    NeighborSegIdList neighborSegIndexList = {.count = 0};
    const NeighborInfo *neighborInfo = NeighborFaceInfoList[segGroupIndex];

    LocalNeighborSegList neighborsAsRelativeAbt = GetLocalSegmentIndexNeighborsAsAbt(n, localSegmentIndex);

    for (int i = 0; i < neighborsAsRelativeAbt.count; i++) {
        const SegmentGroupNeighbor neighbor = neighborsAsRelativeAbt.segGroupNeighborAbt[i].segGroupNeighbor;
        const AbtCoords neighborAbt = neighborsAsRelativeAbt.segGroupNeighborAbt[i].abt;
        switch (neighbor) {
            case SegmentGroupNeighbor_Inside: {
                int neighborSegIndex = ConvertToSegmentIndex2(n, segGroupIndex,
                                                              ConvertToLocalSegmentIndex2(n, neighborAbt));

                neighborSegIndexList.neighborSegId[neighborSegIndexList.count] = neighborSegIndex;
                neighborSegIndexList.count++;
                break;
            }
            case SegmentGroupNeighbor_O:
                neighborSegIndexList.neighborSegId[neighborSegIndexList.count] = ConvertCoordinateByNeighborInfo(
                        baseAxisOrientation, neighborInfo[0], n, neighborAbt);
                neighborSegIndexList.count++;
                break;
            case SegmentGroupNeighbor_A:
                neighborSegIndexList.neighborSegId[neighborSegIndexList.count] = ConvertCoordinateByNeighborInfo(
                        baseAxisOrientation, neighborInfo[1], n, neighborAbt);
                neighborSegIndexList.count++;
                break;
            case SegmentGroupNeighbor_B:
                neighborSegIndexList.neighborSegId[neighborSegIndexList.count] = ConvertCoordinateByNeighborInfo(
                        baseAxisOrientation, neighborInfo[2], n, neighborAbt);
                neighborSegIndexList.count++;
                break;
            case SegmentGroupNeighbor_OA:
            case SegmentGroupNeighbor_OB: {
                NeighborInfo neighbor1Info = GetNeighborInfoOfSegGroupIndex(segGroupIndex, SegmentGroupNeighbor_O);
                AbtCoords neighborAbt1 = ConvertCoordinate2(baseAxisOrientation, neighbor1Info, n, neighborAbt);
                NeighborInfo neighbor2Info = GetNeighborInfoOfSegGroupIndex(segGroupIndex, neighbor);

                neighborSegIndexList.neighborSegId[neighborSegIndexList.count] = ConvertCoordinateByNeighborInfo(
                        baseAxisOrientation, neighbor2Info, n, neighborAbt1);
                neighborSegIndexList.count++;
                break;
            }
            case SegmentGroupNeighbor_AO:
            case SegmentGroupNeighbor_AB: {
                NeighborInfo neighbor1Info = GetNeighborInfoOfSegGroupIndex(segGroupIndex, SegmentGroupNeighbor_A);
                AbtCoords neighborAbt1 = ConvertCoordinate2(baseAxisOrientation, neighbor1Info, n, neighborAbt);
                NeighborInfo neighbor2Info = GetNeighborInfoOfSegGroupIndex(segGroupIndex, neighbor);

                neighborSegIndexList.neighborSegId[neighborSegIndexList.count] = ConvertCoordinateByNeighborInfo(
                        baseAxisOrientation, neighbor2Info, n, neighborAbt1);
                neighborSegIndexList.count++;
                break;
            }
            case SegmentGroupNeighbor_BO:
            case SegmentGroupNeighbor_BA: {
                NeighborInfo neighbor1Info = GetNeighborInfoOfSegGroupIndex(segGroupIndex, SegmentGroupNeighbor_B);
                AbtCoords neighborAbt1 = ConvertCoordinate2(baseAxisOrientation, neighbor1Info, n, neighborAbt);
                NeighborInfo neighbor2Info = GetNeighborInfoOfSegGroupIndex(segGroupIndex, neighbor);

                neighborSegIndexList.neighborSegId[neighborSegIndexList.count] = ConvertCoordinateByNeighborInfo(
                        baseAxisOrientation, neighbor2Info, n, neighborAbt1);
                neighborSegIndexList.count++;
                break;
            }
            case SegmentGroupNeighbor_Outside:
            default:
                neighborSegIndexList.neighborSegId[neighborSegIndexList.count] = INT32_MIN;
                neighborSegIndexList.count++;
        }
    }

    return neighborSegIndexList;
}