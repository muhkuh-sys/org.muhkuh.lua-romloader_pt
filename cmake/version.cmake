#-----------------------------------------------------------------------------
#
# Get the VCS version and store it in the variable PROJECT_VERSION_VCS.
#

# TODO: Check the project's root folder for a ".git", ".hg" or ".svn" folder.
# For now we know that this project uses GIT.
IF(DEFINED ENV{VCS_GIT_DESCRIBE_OUTPUT})
	SET(VCS_GIT_DESCRIBE_OUTPUT "$ENV{VCS_GIT_DESCRIBE_OUTPUT}")
	MESSAGE("Using pre-defined ENV variable VCS_GIT_DESCRIBE_OUTPUT: ${VCS_GIT_DESCRIBE_OUTPUT}")
ELSEIF(DEFINED VCS_GIT_DESCRIBE_OUTPUT)
	SET(VCS_GIT_DESCRIBE_OUTPUT "${VCS_GIT_DESCRIBE_OUTPUT}")
	MESSAGE("Using pre-defined CMake variable VCS_GIT_DESCRIBE_OUTPUT: ${VCS_GIT_DESCRIBE_OUTPUT}")
ELSE(DEFINED ENV{VCS_GIT_DESCRIBE_OUTPUT})
	FIND_PACKAGE(Git)
	IF(GIT_FOUND)
		# Run this command in the project root folder. The build folder might be somewhere else.
		EXECUTE_PROCESS(COMMAND ${GIT_EXECUTABLE} describe --abbrev=12 --always --dirty=+
		                WORKING_DIRECTORY ${CMAKE_HOME_DIRECTORY}
		                RESULT_VARIABLE VCS_VERSION_RESULT
		                OUTPUT_VARIABLE VCS_VERSION_OUTPUT)

		IF(VCS_VERSION_RESULT EQUAL 0)
			STRING(STRIP "${VCS_VERSION_OUTPUT}" VCS_GIT_DESCRIBE_OUTPUT)
		ENDIF(VCS_VERSION_RESULT EQUAL 0)
	ENDIF(GIT_FOUND)
ENDIF(DEFINED ENV{VCS_GIT_DESCRIBE_OUTPUT})

IF(DEFINED VCS_GIT_DESCRIBE_OUTPUT)
	STRING(REGEX MATCH "^[0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f]\\+?$" MATCH ${VCS_GIT_DESCRIBE_OUTPUT})
	IF(NOT MATCH STREQUAL "")
		# This is a repository with no tags. Use the raw SHA sum.
		SET(PROJECT_VERSION_VCS_VERSION ${MATCH})
	ELSE(NOT MATCH STREQUAL "")
		STRING(REGEX MATCH "^v([0-9]+\\.[0-9]+\\.[0-9]+)$" MATCH ${VCS_GIT_DESCRIBE_OUTPUT})
		IF(NOT MATCH STREQUAL "")
			# This is a repository which is exactly on a tag. Use the tag name.
			SET(PROJECT_VERSION_VCS_VERSION ${CMAKE_MATCH_1})
		ELSE(NOT MATCH STREQUAL "")
			STRING(REGEX MATCH "^v[0-9]+\\.[0-9]+\\.[0-9]+-[0-9]+-g([0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f]\\+?)$" MATCH ${VCS_GIT_DESCRIBE_OUTPUT})
			IF(NOT MATCH STREQUAL "")
				# This is a repository with commits after the last tag.
				SET(PROJECT_VERSION_VCS_VERSION ${CMAKE_MATCH_1})
			ELSE(NOT MATCH STREQUAL "")
				# The description has an unknown format.
				SET(PROJECT_VERSION_VCS_VERSION ${VCS_GIT_DESCRIBE_OUTPUT})
			ENDIF(NOT MATCH STREQUAL "")
		ENDIF(NOT MATCH STREQUAL "")
	ENDIF(NOT MATCH STREQUAL "")

	STRING(CONCAT PROJECT_VERSION_VCS "GIT" "${PROJECT_VERSION_VCS_VERSION}")
ELSE(DEFINED VCS_GIT_DESCRIBE_OUTPUT)
	SET(PROJECT_VERSION_VCS unknown)
ENDIF(DEFINED VCS_GIT_DESCRIBE_OUTPUT)

MESSAGE("PROJECT_VERSION_VCS: ${PROJECT_VERSION_VCS}")
