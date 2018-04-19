# @author Gvozden Neskovic
# @brief  DataDistribution tests: DPL integration using data model adapter

o2_define_bucket(
    NAME
    SubTimeFrameDplAdapter_bucket

    DEPENDENCIES
    DataDistributionCommon
    O2Device
    FrameworkApplication_bucket
)

set(MODULE_NAME "SubTimeFrameDplAdapter")
set(MODULE_BUCKET_NAME SubTimeFrameDplAdapter_bucket)

O2_SETUP(NAME ${MODULE_NAME})


set(SRCS
)

set(LIBRARY_NAME ${MODULE_NAME})
set(BUCKET_NAME ${MODULE_BUCKET_NAME})

#O2_GENERATE_LIBRARY()

set(LIBRARY_NAME ${MODULE_NAME})
set(BUCKET_NAME ${MODULE_BUCKET_NAME})

# SubTimeFrameBuilderDevice exe

set(EXE_SRC
    test_StfDplAdapter.cxx
)

O2_GENERATE_EXECUTABLE(
    EXE_NAME "test_StfDplAdapter"
    SOURCES ${EXE_SRC}
    BUCKET_NAME ${MODULE_BUCKET_NAME}
)


configure_file(${CMAKE_CURRENT_SOURCE_DIR}/start_StfDplAdapterTest.sh.in
  ${CMAKE_BINARY_DIR}/bin/start_StfDplAdapterTest.sh)


install(FILES ${CMAKE_BINARY_DIR}/bin/start_StfDplAdapterTest.sh
  DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
  PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ WORLD_READ)
