#!/usr/bin/make -f
#
# Provides targets to build code with SDCC, generate documentation etc.
#
# | Target            | Function                                          |
# |-------------------|---------------------------------------------------|
# | build (default)   | Builds a .hex file and dependencies               |
# | all               | Builds a .hex file and every .c library           |
# | dbc               | Builds C headers from Vector dbc files            |
# | debug             | Builds for debugging with sdcdb                   |
# | printEnv          | Used by scripts to determine project settings     |
# | uVision           | Run uVisionupdate.sh                              |
# | html              | Build all html documentation under doc/           |
# | pdf               | Build all pdf documentation under doc/            |
# | gh-pages          | Assemble all docs for GitHub Pages publishing     |
# | clean-build       | Remove build output                               |
# | clean-doc         | Remove doxygen output for user doc                |
# | clean             | Clean everything                                  |
#
# Override the following settings in Makefile.local if needed.
#
# | Assignment        | Function                                          |
# |-------------------|---------------------------------------------------|
# | AWK               | The awk interpreter                               |
# | BUILDDIR          | SDCC output directory                             |
# | CC                | Compiler                                          |
# | CFLAGS            | Compiler flags                                    |
# | CPP               | C preprocesser used by several scripts            |
# | CONFDIR           | Location for configuration files                  |
# | CANPROJDIR        | Path to the CAN project                           |
# | DBCDIR            | Location for generated DBC headers                |
# | DOC_ALL_TARGETS   | All doc/ subtargets (user, dev, dbc, scripts)     |
# | DOC_PUB_TARGETS   | All gh-pages/ subtargets (user, dev, scripts)     |
# | GENDIR            | Location for generated code                       |
# | INCDIR            | Include directory for contributed headers         |
# | OBJSUFX           | The file name suffix for object files             |
# | HEXSUFX           | The file name suffix for intel hex files          |
# | DATE              | System date, used if `git` cannot be found        |
# | VERSION           | The `git` version of the project or the date      |
# | PROJECT           | The name of this project                          |
#

# Include shared defaults
include Makefile.include

# Documentation subtargets
DOC_ALL_TARGETS=user dev dbc scripts

# Documenation to publish on GitHub Pages
DOC_PUB_TARGETS=user dev scripts

# List of public source files, for generating the user documentation.
USERSRC:=	$(shell find src/ -name \*.h -o -name main.c -o -name \*.md -o -name examples)
USERSRC!=	find src/ -name \*.h -o -name main.c -o -name \*.md -o -name examples

# List of all source files for generating the developer documentation.
DEVSRC:=	$(shell find src/ -name \*.\[hc] -o -name \*.md -o -name examples)
DEVSRC!=	find src/ -name \*.\[hc] -o -name \*.md -o -name examples

# List of script files for generating documentation
SCRIPTSRC:=	$(shell find scripts/ -name \*.awk -o -name \*.sh -o -name \*.md)
SCRIPTSRC!=	find scripts/ -name \*.awk -o -name \*.sh -o -name \*.md
SCRIPTSRC+=	Makefile* uVisionupdate.sh

#
# No more overrides.
#

# Local config
_LOCAL_MK:=	$(shell test -f Makefile.local || touch Makefile.local)
_LOCAL_MK!=	test -f Makefile.local || touch Makefile.local ; echo

# Gmake style, works with FreeBSD make, too
include Makefile.local

build:

.PHONY: ${GENDIR}/sdcc.mk ${GENDIR}/dbc.mk ${GENDIR}/build.mk

# Configure SDCC
${GENDIR}/sdcc.mk: ${GENDIR}
	@env CC="${CC}" sh scripts/sdcc.sh ${CONFDIR}/sdcc > $@

# Generate dbc
${GENDIR}/dbc.mk: ${GENDIR}
	@sh scripts/dbc.sh ${CANPROJDIR}/ > $@

# Generate build
${GENDIR}/build.mk: dbc ${GENDIR}
	@env CPP="${CPP}" \
	     ${AWK} -f scripts/build.awk \
	            -vOBJSUFX="${OBJSUFX}" -vBINSUFX="${HEXSUFX}" \
	            src/ -I${INCDIR}/ -I${GENDIR}/ -DSDCC > $@

.PHONY: build all debug dbc

# Generate headers from CANdbs
dbc: ${GENDIR}/dbc.mk
	@${MAKE} DBCDIR=${DBCDIR} -f ${GENDIR}/dbc.mk $@

${DBCDIR}: dbc

# Perform build stage
build all: ${GENDIR}/sdcc.mk ${GENDIR}/build.mk dbc
	@env CC="${CC}" CFLAGS="${CFLAGS}" OBJDIR="${BUILDDIR}/" \
	     ${MAKE} -rf ${GENDIR}/sdcc.mk -f ${GENDIR}/build.mk $@

debug: ${GENDIR}/sdcc.mk ${GENDIR}/build.mk dbc
	@env CC="${CC}" CFLAGS="${CFLAGS} --debug" OBJDIR="${BUILDDIR}/" \
	     ${MAKE} -rf ${GENDIR}/sdcc.mk -f ${GENDIR}/build.mk build

.PHONY: printEnv uVision µVision

printEnv:
	@echo export PROJECT=\"${PROJECT}\"
	@echo export CANPROJDIR=\"${CANPROJDIR}\"
	@echo export GENDIR=\"${GENDIR}\"
	@echo export INCDIR=\"${INCDIR}\"
	@echo export CPP=\"${CPP}\"
	@echo export AWK=\"${AWK}\"

uVision µVision:
	@sh uVisionupdate.sh

# Documentation sources
doc/user:    ${USERSRC}
doc/dev:     ${DEVSRC}
doc/dbc:     ${DBCDIR}
doc/scripts: ${SCRIPTSRC}

# Doxygen targets
${DOC_ALL_TARGETS:C,^,doc/,}: ${CONFDIR}/doxygen.common ${CONFDIR}/doxygen.${.TARGET:T}
	@rm -rf "${.TARGET}"
	@mkdir -p "${.TARGET}"
	@echo 'PROJECT_NAME="${PROJECT}-${.TARGET:T}"'  >> "${.TARGET}"/.conf
	@echo 'PROJECT_NUMBER=${VERSION}'               >> "${.TARGET}"/.conf
	@echo 'OUTPUT_DIRECTORY="${.TARGET}/"'          >> "${.TARGET}"/.conf
	@echo 'WARN_LOGFILE="${.TARGET}/warnings.log"'  >> "${.TARGET}"/.conf
	@cat ${CONFDIR}/doxygen.common ${CONFDIR}/doxygen.${.TARGET:T} \
	     ${.TARGET}/.conf | doxygen -

# PDF targets
${DOC_ALL_TARGETS:C,^,doc/,:C,$$,/latex/refman.pdf,}: ${.TARGET:H:H}
	@cd "${.TARGET:H}" && ${MAKE}

# GitHub Pages targets
${DOC_PUB_TARGETS:C,^,gh-pages/,}: doc/${.TARGET:T} doc/${.TARGET:T}/latex/refman.pdf
	@rm -rf "${.TARGET}"
	@cp -r "doc/${.TARGET:T}/html" "${.TARGET}"
	@cp "${.ALLSRC:[-1]}" "${.TARGET}/${PROJECT}-${.TARGET:T}.pdf"

# Documentation meta targets
html: ${DOC_ALL_TARGETS:C,^,doc/,}
pdf: ${DOC_ALL_TARGETS:C,^,doc/,:C,$$,/latex/refman.pdf,}
gh-pages: ${DOC_PUB_TARGETS:C,^,gh-pages/,}

.PHONY: clean clean-doc clean-build

clean: clean-doc clean-build

clean-doc:
	@rm -rf doc/*

clean-build:
	@rm -rf ${BUILDDIR}/* ${GENDIR}/*
