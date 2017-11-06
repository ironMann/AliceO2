# @author Gvozden Neskovic
# @brief  DataDistribution common stuff: DataModel and Utilities

set(MODULE_NAME "DataDistributionCommon")
set(MODULE_BUCKET_NAME DataDistributionCommon_bucket)

O2_SETUP(NAME ${MODULE_NAME})

set(SRCS
    src/ReadoutDataModel.cxx
    src/SubTimeFrameDataModel.cxx
    src/SubTimeFrameVisitors.cxx
  )

set(LIBRARY_NAME ${MODULE_NAME})
set(BUCKET_NAME ${MODULE_BUCKET_NAME})

O2_GENERATE_LIBRARY()


set(TEST_SRCS
  test/testO2ExtentVector.cxx
)

O2_GENERATE_TESTS(
  MODULE_LIBRARY_NAME ${LIBRARY_NAME}
  BUCKET_NAME ${BUCKET_NAME}
  TEST_SRCS ${TEST_SRCS}
)
