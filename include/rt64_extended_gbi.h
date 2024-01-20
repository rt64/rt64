//
// RT64
//

#ifndef RT64_EXTENDED_GBI
#define RT64_EXTENDED_GBI

// G_SPNOOP on F3D and F3DEX2.
#ifdef F3DEX_GBI_2
#	define RT64_HOOK_OPCODE			0xE0
#else
#	define RT64_HOOK_OPCODE			0x00
#endif
#define RT64_HOOK_OP_ENABLE			0x0
#define RT64_HOOK_OP_DISABLE		0x1
#define RT64_HOOK_OP_DL				0x2
#define RT64_HOOK_OP_BRANCH			0x3

// 0x5254 for ASCII "RT" followed by 0x64.
#define RT64_HOOK_MAGIC_NUMBER		0x525464

#ifndef RT64_EXTENDED_OPCODE
// TODO: Define defaults here based on the GBI.
#	define RT64_EXTENDED_OPCODE		0x64
#endif

#define G_EX_NOOP					0x000000
#define G_EX_PRINT					0x000001
#define G_EX_TEXRECT_V1				0x000002
#define G_EX_FILLRECT_V1			0x000003
#define G_EX_SETVIEWPORT_V1			0x000004
#define G_EX_SETSCISSOR_V1			0x000005
#define G_EX_SETRECTALIGN_V1		0x000006
#define G_EX_SETVIEWPORTALIGN_V1	0x000007
#define G_EX_SETSCISSORALIGN_V1		0x000008
#define G_EX_SETREFRESHRATE_V1		0x000009
#define G_EX_VERTEXZTEST_V1			0x00000A
#define G_EX_ENDVERTEXZTEST_V1		0x00000B
#define G_EX_MATRIXGROUP_V1			0x00000C
#define G_EX_POPMATRIXGROUP_V1		0x00000D
#define G_EX_FORCEUPSCALE2D_V1		0x00000E
#define G_EX_FORCETRUEBILERP_V1		0x00000F
#define G_EX_FORCESCALELOD_V1		0x000010
#define G_EX_FORCEBRANCH_V1			0x000011
#define G_EX_MAX					0x000012

#define G_EX_ORIGIN_NONE			0x800
#define G_EX_ORIGIN_LEFT			0x0
#define G_EX_ORIGIN_CENTER			0x200
#define G_EX_ORIGIN_RIGHT			0x400

#define G_EX_NOPUSH					0x0
#define G_EX_PUSH					0x1

#define G_EX_ID_IGNORE				0x0
#define G_EX_ID_AUTO				0xFFFFFFFF

#define G_EX_COMPONENT_SKIP			0x0
#define G_EX_COMPONENT_INTERPOLATE	0x1
#define G_EX_COMPONENT_AUTO			0x2

#define G_EX_ORDER_LINEAR			0x0
#define G_EX_ORDER_AUTO				0x1

#define G_EX_BILERP_NONE			0x0
#define G_EX_BILERP_ONLY			0x1
#define G_EX_BILERP_ALL				0x2

// Represents the 8-byte commands in the F3D microcode family
typedef union {
	struct {
		unsigned word0;
		unsigned word1;
	} values;
	unsigned long long dummy; // Force to 8-byte alignment
} GfxCommand;

#define PARAM(value, bits, shift) \
    ((unsigned) (((unsigned)(value) & ((1U << (bits)) - 1U)) << (shift)))

#define DOWHILE(code) \
	do { code } while (0)

#define G_EX_WRITECOMMAND(cmd, _word0, _word1) \
	{ \
        cmd->values.word0 = _word0; \
        cmd->values.word1 = _word1; \
	}

#define G_EX_COMMAND1(cmd, _word0, _word1) \
    DOWHILE( \
		GfxCommand *_cmd = (GfxCommand*)(cmd); \
		G_EX_WRITECOMMAND((_cmd + 0), _word0, _word1) \
	)
	
#define G_EX_COMMAND2(cmd, _word0, _word1, _word2, _word3) \
    DOWHILE( \
		GfxCommand *_cmd = (GfxCommand*)(cmd); \
		(void)(cmd); \
		G_EX_WRITECOMMAND((_cmd + 0), _word0, _word1) \
		G_EX_WRITECOMMAND((_cmd + 1), _word2, _word3) \
	)
	
#define G_EX_COMMAND3(cmd, _word0, _word1, _word2, _word3, _word4, _word5) \
    DOWHILE( \
		GfxCommand *_cmd = (GfxCommand*)(cmd); \
		(void)(cmd); \
		(void)(cmd); \
		G_EX_WRITECOMMAND((_cmd + 0), _word0, _word1) \
		G_EX_WRITECOMMAND((_cmd + 1), _word2, _word3) \
		G_EX_WRITECOMMAND((_cmd + 2), _word4, _word5) \
	)
	
#define G_EX_COMMAND4(cmd, _word0, _word1, _word2, _word3, _word4, _word5, _word6, _word7) \
    DOWHILE( \
		GfxCommand *_cmd = (GfxCommand*)(cmd); \
		(void)(cmd); \
		(void)(cmd); \
		(void)(cmd); \
		G_EX_WRITECOMMAND(cmd_, _word0, _word1) \
		G_EX_WRITECOMMAND(cmd_, _word2, _word3) \
		G_EX_WRITECOMMAND(cmd_, _word4, _word5) \
		G_EX_WRITECOMMAND(cmd_, _word6, _word7) \
	)

#define gEXEnable(cmd) \
	G_EX_COMMAND1(cmd, \
		PARAM(RT64_HOOK_OPCODE, 8, 24) | PARAM(RT64_HOOK_MAGIC_NUMBER, 24, 0), \
		PARAM(RT64_HOOK_OP_ENABLE, 4, 28) | PARAM(RT64_EXTENDED_OPCODE, 8, 0))

#define gEXDisable(cmd) \
	G_EX_COMMAND1(cmd, \
		PARAM(RT64_HOOK_OPCODE, 8, 24) | PARAM(RT64_HOOK_MAGIC_NUMBER, 24, 0), \
		PARAM(RT64_HOOK_OP_DISABLE, 4, 28))

#define gEXBranchList(cmd, dlist) \
	G_EX_COMMAND1(cmd, \
		PARAM(RT64_HOOK_OPCODE, 8, 24) | PARAM(RT64_HOOK_MAGIC_NUMBER, 24, 0), \
		PARAM(RT64_HOOK_OP_BRANCH, 4, 28) | PARAM(dlist, 28, 0))

#define gEXDisplayList(cmd, dlist) \
	G_EX_COMMAND1(cmd, \
		PARAM(RT64_HOOK_OPCODE, 8, 24) | PARAM(RT64_HOOK_MAGIC_NUMBER, 24, 0), \
		PARAM(RT64_HOOK_OP_DL, 4, 28) | PARAM(dlist, 28, 0))

#define gEXNoOp(cmd) \
	G_EX_COMMAND1(cmd, \
		PARAM(RT64_EXTENDED_OPCODE, 8, 24) | PARAM(G_EX_NOOP, 24, 0), \
		0)

#define gEXPrint(cmd) \
	G_EX_COMMAND1(cmd, \
		PARAM(RT64_EXTENDED_OPCODE, 8, 24) | PARAM(G_EX_PRINT, 24, 0), \
		0)

#define gEXTextureRectangle(cmd, lorigin, rorigin, ulx, uly, lrx, lry, tile, s, t, dsdx, dtdy) \
	G_EX_COMMAND3(cmd, \
		PARAM(RT64_EXTENDED_OPCODE, 8, 24) | PARAM(G_EX_TEXRECT_V1, 24, 0), \
		PARAM(tile, 3, 0) | PARAM(lorigin, 12, 3) | PARAM(rorigin, 12, 15) | PARAM(0, 5, 27), \
		\
		PARAM(ulx, 16, 16) | PARAM(uly, 16, 0), \
		PARAM(lrx, 16, 16) | PARAM(lry, 16, 0), \
		\
		PARAM(s, 16, 16) | PARAM(t, 16, 0), \
		PARAM(dsdx, 16, 16) | PARAM(dtdy, 16, 0) \
	)
	
#define gEXViewport(cmd, origin, vp) \
	G_EX_COMMAND2(cmd, \
		PARAM(RT64_EXTENDED_OPCODE, 8, 24) | PARAM(G_EX_SETVIEWPORT_V1, 24, 0), \
		PARAM(origin, 12, 0), \
		\
		0, \
		(unsigned)vp \
	)

#define gEXSetScissor(cmd, mode, lorigin, rorigin, ulx, uly, lrx, lry) \
	G_EX_COMMAND2(cmd, \
		PARAM(RT64_EXTENDED_OPCODE, 8, 24) | PARAM(G_EX_SETSCISSOR_V1, 24, 0), \
		PARAM(mode, 2, 0) | PARAM(lorigin, 12, 2) | PARAM(rorigin, 12, 14), \
		\
		PARAM((ulx) * 4, 16, 16) | PARAM((uly) * 4, 16, 0), \
		PARAM((lrx) * 4, 16, 16) | PARAM((lry) * 4, 16, 0) \
	)

#define gEXSetRectAlign(cmd, lorigin, rorigin, ulxOffset, ulyOffset, lrxOffset, lryOffset) \
	G_EX_COMMAND2(cmd, \
		PARAM(RT64_EXTENDED_OPCODE, 8, 24) | PARAM(G_EX_SETRECTALIGN_V1, 24, 0), \
		PARAM(lorigin, 12, 0) | PARAM(rorigin, 12, 12), \
		\
		PARAM((ulxOffset), 16, 16) | PARAM((ulyOffset), 16, 0), \
		PARAM((lrxOffset), 16, 16) | PARAM((lryOffset), 16, 0) \
	)

#define gEXSetViewportAlign(cmd, origin, xOffset, yOffset) \
	G_EX_COMMAND2(cmd, \
		PARAM(RT64_EXTENDED_OPCODE, 8, 24) | PARAM(G_EX_SETVIEWPORTALIGN_V1, 24, 0), \
		PARAM(origin, 12, 0), \
		\
		PARAM((xOffset), 16, 16) | PARAM((yOffset), 16, 0), \
		0 \
	)

#define gEXSetScissorAlign(cmd, lorigin, rorigin, ulxOffset, ulyOffset, lrxOffset, lryOffset, ulxBound, ulyBound, lrxBound, lryBound) \
	G_EX_COMMAND3(cmd, \
		PARAM(RT64_EXTENDED_OPCODE, 8, 24) | PARAM(G_EX_SETSCISSORALIGN_V1, 24, 0), \
		PARAM(lorigin, 12, 0) | PARAM(rorigin, 12, 12), \
		\
		PARAM((ulxOffset) * 4, 16, 16) | PARAM((ulyOffset) * 4, 16, 0), \
		PARAM((lrxOffset) * 4, 16, 16) | PARAM((lryOffset) * 4, 16, 0), \
		\
		PARAM((ulxBound) * 4, 16, 16) | PARAM((ulyBound) * 4, 16, 0), \
		PARAM((lrxBound) * 4, 16, 16) | PARAM((lryBound) * 4, 16, 0) \
	)

#define gEXSetRefreshRate(cmd, refresh_rate) \
	G_EX_COMMAND1(cmd, \
		PARAM(RT64_EXTENDED_OPCODE, 8, 24) | PARAM(G_EX_SETREFRESHRATE_V1, 24, 0), \
		PARAM(refresh_rate, 16, 0) \
	)

#define gEXVertexZTest(cmd, vertex_index) \
	G_EX_COMMAND1(cmd, \
		PARAM(RT64_EXTENDED_OPCODE, 8, 24) | PARAM(G_EX_VERTEXZTEST_V1, 24, 0), \
		PARAM(vertex_index, 8, 0) \
	)

#define gEXEndVertexZTest(cmd) \
	G_EX_COMMAND1(cmd, \
		PARAM(RT64_EXTENDED_OPCODE, 8, 24) | PARAM(G_EX_ENDVERTEXZTEST_V1, 24, 0), \
		0 \
	)

#define gEXMatrixGroup(cmd, id, push, proj, pos, rot, scale, vert, order) \
	G_EX_COMMAND2(cmd, \
		PARAM(RT64_EXTENDED_OPCODE, 8, 24) | PARAM(G_EX_MATRIXGROUP_V1, 24, 0), \
		id, \
		\
		PARAM(push, 1, 0) | PARAM((proj) != 0, 1, 1) | PARAM(pos, 2, 2) | PARAM(rot, 2, 4) | PARAM(scale, 2, 6) | PARAM(vert, 2, 8) | PARAM(order, 2, 10), \
		0 \
	)

#define gEXPopMatrixGroup(cmd) \
	G_EX_COMMAND1(cmd, \
		PARAM(RT64_EXTENDED_OPCODE, 8, 24) | PARAM(G_EX_POPMATRIXGROUP_V1, 24, 0), \
		PARAM(1, 8, 0) \
	)

#define gEXPopMatrixGroupN(cmd, count) \
	G_EX_COMMAND1(cmd, \
		PARAM(RT64_EXTENDED_OPCODE, 8, 24) | PARAM(G_EX_POPMATRIXGROUP_V1, 24, 0), \
		PARAM(count, 8, 0) \
	)

#define gEXForceUpscale2D(cmd, force) \
	G_EX_COMMAND1(cmd, \
		PARAM(RT64_EXTENDED_OPCODE, 8, 24) | PARAM(G_EX_FORCEUPSCALE2D_V1, 24, 0), \
		PARAM(force, 1, 0) \
	)

#define gEXForceTrueBilerp(cmd, mode) \
	G_EX_COMMAND1(cmd, \
		PARAM(RT64_EXTENDED_OPCODE, 8, 24) | PARAM(G_EX_FORCETRUEBILERP_V1, 24, 0), \
		PARAM(mode, 2, 0) \
	)

#define gEXForceScaleLOD(cmd, force) \
	G_EX_COMMAND1(cmd, \
		PARAM(RT64_EXTENDED_OPCODE, 8, 24) | PARAM(G_EX_FORCESCALELOD_V1, 24, 0), \
		PARAM(force, 1, 0) \
	)

#define gEXForceBranch(cmd, force) \
	G_EX_COMMAND1(cmd, \
		PARAM(RT64_EXTENDED_OPCODE, 8, 24) | PARAM(G_EX_FORCEBRANCH_V1, 24, 0), \
		PARAM(force, 1, 0) \
	)

#endif // RT64_EXTENDED_GBI
