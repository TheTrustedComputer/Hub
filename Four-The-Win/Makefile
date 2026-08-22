############################
## Four the Win! Makefile ##
############################

CC_PROG = gcc
CC_MING = x86_64-w64-mingw32-gcc
CC_WRES = x86_64-w64-mingw32-windres
CC_ARCH = -march=native # Redefine to target a different architecture
CC_WARN = -Wall -Wextra
CC_OPTS = -O3 -pipe -flto -ffast-math -fno-trapping-math -fno-signed-zeros -fassociative-math -freciprocal-math -fsingle-precision-constant -fprefetch-loop-arrays -fno-stack-protector -fno-stack-check -fno-stack-clash-protection -fmerge-all-constants -fshort-enums -funroll-loops -ffunction-sections -fdata-sections -Wl,--gc-sections $(CC_ARCH) -s -w
CC_DBUG = -O0 -pipe -g -pg $(CC_ARCH) -DFTW_DEBUG
CC_LIBS = -pthread -lm
CC_ASAN = -fsanitize=address,undefined,leak
CC_THRD = -fsanitize=thread
CC_CSTD = -std=c23
CC_EXTR = # Place extra compiler flags here, or append them to the `make` command

FTW_BIN = FourTheWin
FTW_WEB = $(CC_PROG) $(CC_OPTS) $(CC_CSTD) FourTheWin.c -o Web/$(FTW_BIN) $(CC_LIBS) $(CC_EXTR)
FTW_REL = $(CC_PROG) $(CC_OPTS) $(CC_CSTD) FourTheWin.c -o Release/$(FTW_BIN) $(CC_LIBS) $(CC_EXTR)
FTW_DBG = $(CC_PROG) $(CC_WARN) $(CC_DBUG) $(CC_CSTD) FourTheWin.c -o Debug/$(FTW_BIN) $(CC_LIBS) $(CC_EXTR)
FTW_VAR = Connect4 # Variant to instrument for PGO (Connect4/Make7)

##########################
## Compile-time options ##
##########################

FTW_SLIDERS_OFF = # Do not use the sliding window technique when evaluating Make 7 positions
FTW_SCC_ON = # Employ SCC-based reasoning to resolve true, cyclic positions as proven draws
FTW_LIBDIVIDE_ON = # Utilize libdivide to optimize quotients with run-time constant divisors
FTW_LIBDIVIDE_BL = # Exchange the branchful libdivide structure with a branchless one
FTW_FASTMOD_ON = # Accelerate modulo operations with Daniel Lemire's fastmod algorithm
FTW_SQLITE_ON = # Enable SQLite to cache previously computed solutions in the database
FTW_CUSTOM = # Specify a custom board size for analysis (default: system's long bit width)
FTW_WORD_SIZE = $(shell getconf LONG_BIT) # Number of bits to represent the game board
FTW_WEBASSEMBLY = # Compile for the WebAssembly target instead of the host architecture

ifdef FTW_SLIDERS_OFF
	CC_EXTR += -DFTW_NO_SLIDER
endif

ifdef FTW_SCC_ON
	CC_EXTR += -DFTW_SCC
endif

ifdef FTW_LIBDIVIDE_ON
	CC_EXTR += -DFTW_LIBDIVIDE
	ifdef FTW_LIBDIVIDE_BL
		CC_EXTR += -DFTW_BRANCHLESS
	endif
endif

ifdef FTW_FASTMOD_ON
	CC_EXTR += -DFTW_FASTMOD
endif

ifdef FTW_CUSTOM
	CC_EXTR += -DFTW_INT_WIDTH=$(FTW_WORD_SIZE)
endif

ifdef FTW_WEBASSEMBLY
	CC_PROG = emcc
	FTW_BIN = FourTheWin.js
	EMCC_RUNTIMES = '["cwrap"]'
	EMCC_FUNCTIONS = '["_FourTheWin_C4_cols", \
					   "_FourTheWin_C4_rows", \
					   "_FourTheWin_C4_prepare", \
					   "_FourTheWin_C4_init", \
					   "_FourTheWin_C4_reset", \
					   "_FourTheWin_C4_status", \
					   "_FourTheWin_C4_play", \
					   "_FourTheWin_C4_undo", \
					   "_FourTheWin_C4_playerMask", \
					   "_FourTheWin_C4_occupancyMask", \
					   "_FourTheWin_C4_owner", \
					   "_FourTheWin_varID", \
					   "_FourTheWin_varName"]'
	CC_OPTS += -s ASSERTIONS=0 -s MEMORY64=1 -s ALLOW_MEMORY_GROWTH=1 -s INVOKE_RUN=0 -s EXPORTED_RUNTIME_METHODS=$(EMCC_RUNTIMES) -s EXPORTED_FUNCTIONS=$(EMCC_FUNCTIONS)

	ifdef FTW_SQLITE_ON
		CC_OPTS += -s USE_SQLITE3=1 -DFTW_SQLITE
	endif

	CC_ARCH =
	CC_CSTD =
else
	ifdef FTW_SQLITE_ON
		CC_EXTR += -DFTW_SQLITE -lsqlite3
	endif
endif

release:
	@mkdir -p Release
ifdef FTW_WEBASSEMBLY
	$(FTW_WEB)
else
	$(FTW_REL)
endif

release-asan:
	@mkdir -p Release
	$(FTW_REL) $(CC_ASAN)

release-pgo: clean
	@mkdir -p Release
ifeq ($(CC_PROG), gcc)
	$(FTW_REL) -DFTW_PGO -fprofile-generate=$(FTW_BIN)_gccprof
else ifeq ($(CC_PROG), clang)
	$(FTW_REL) -DFTW_PGO -fprofile-instr-generate=$(FTW_BIN)_clangprof
endif
	@Release/"$(FTW_BIN)" -v $(FTW_VAR)
ifeq ($(CC_PROG), gcc)
	$(FTW_REL) -fprofile-use=$(FTW_BIN)_gccprof
else ifeq ($(CC_PROG), clang)
	llvm-profdata merge $(FTW_BIN)_clangprof -o $(FTW_BIN).profdata
	$(FTW_REL) -fprofile-instr-use=$(FTW_BIN).profdata
endif

#release-bolt: clean
#	perf record -e instructions,cycles,stalled-cycles-frontend,branches,branch-misses,cache-references,cache-misses -- ./"$(FTW_BIN)"
#	perf2bolt -nl "$(FTW_BIN)" -p perf.data -o $(FTW_BIN)_perf.fdata
#	llvm-bolt "$(FTW_BIN)" -o "$(FTW_BIN)_bolt" -reorder-blocks=ext-tsp -split-functions -split-all-cold -split-eh -dyno-stats

debug:
	@mkdir -p Debug
	$(FTW_DBG)

debug-asan:
	@mkdir -p Debug
	$(FTW_DBG) $(CC_ASAN)

clean:
	rm -rf gmon.out perf.data *.fdata *_gccprof *_clangprof *.profdata *.gcda *.db*
