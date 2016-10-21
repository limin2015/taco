#include "test.h"
#include "test_tensors.h"

#include <map>

#include "tensor.h"
#include "expr.h"
#include "format.h"
#include "packed_tensor.h"
#include "util/strings.h"

using namespace taco;

struct TestData {
  TestData(Tensor<double> tensor,
           const PackedTensor::Indices& expectedIndices,
           const vector<double> expectedValues)
      : tensor(tensor),
        expectedIndices(expectedIndices), expectedValues(expectedValues) {
  }

  Tensor<double>        tensor;
  PackedTensor::Indices expectedIndices;
  vector<double>        expectedValues;
};

ostream &operator<<(ostream& os, const TestData& data) {
  os << util::join(data.tensor.getDimensions(), "x")
     << " (" << data.tensor.getFormat() << ")";
  return os;
}

struct storage : public TestWithParam<TestData> {};

TEST_P(storage, pack) {
  Tensor<double> tensor = GetParam().tensor;

  auto tensorPack = tensor.getPackedTensor();
  ASSERT_NE(nullptr, tensorPack);

  // Check that the indices are as expected
  auto& expectedIndices = GetParam().expectedIndices;
  auto&         indices = tensorPack->getIndices();
  ASSERT_EQ(expectedIndices.size(), indices.size());

  for (size_t i=0; i < indices.size(); ++i) {
    auto expectedIndex = expectedIndices[i];
    auto         index = indices[i];
    ASSERT_EQ(expectedIndex.size(), index.size());
    for (size_t j=0; j < index.size(); ++j) {
      ASSERT_VECTOR_EQ(expectedIndex[j], index[j]);
    }
  }

  auto& expectedValues = GetParam().expectedValues;
  ASSERT_EQ(expectedValues.size(), tensorPack->getNnz());
  auto values = tensorPack->getValues();
  ASSERT_VECTOR_EQ(expectedValues, values);
}

INSTANTIATE_TEST_CASE_P(vector, storage,
                        Values(TestData(d1a("a", "d"),
                                        {
                                          {
                                            // Dense index
                                          }
                                        },
                                        {1}
                                        ),
                               TestData(d1a("a", "s"),
                                        {
                                          {
                                            // Sparse index
                                            {0,1},
                                            {0}
                                          }
                                        },
                                        {1}
                                        ),
                               TestData(d5a("a", "d"),
                                        {
                                          {
                                            // Dense index
                                          }
                                        },
                                        {0, 1, 0, 0, 2}
                                        ),
                               TestData(d5a("a", "s"),
                                        {
                                          {
                                            // Sparse index
                                            {0,2},
                                            {1,4}
                                          },
                                        },
                                        {1, 2}
                                        )
                               )
                        );

INSTANTIATE_TEST_CASE_P(matrix, storage,
                        Values(TestData(d33a("A", "dd"),
                                        {
                                          {
                                            // Dense index
                                          },
                                          {
                                            // Dense index
                                          }
                                        },
                                        {0, 1, 0,
                                         0, 0, 0,
                                         2, 0, 3}
                                        ),
                               TestData(d33a("A", "sd"),  // Blocked svec
                                        {
                                          {
                                            // Sparse index
                                            {0, 2},
                                            {0, 2},
                                          },
                                          {
                                            // Dense index
                                          }
                                        },
                                        {0, 1, 0,
                                          2, 0, 3}
                                        ),
                               TestData(d33a("A", "ds"),  // CSR
                                        {
                                          {
                                            // Dense index
                                          },
                                          {
                                            // Sparse index
                                            {0, 1, 1, 3},
                                            {1, 0, 2},
                                          }
                                        },
                                        {1, 2, 3}
                                        ),
                               TestData(d33a("A", "ss"),  // DCSR
                                        {
                                          {
                                            // Sparse index
                                            {0, 2},
                                            {0, 2},
                                          },
                                          {
                                            // Sparse index
                                            {0, 1, 3},
                                            {1, 0, 2},
                                          }
                                        },
                                        {1, 2, 3}
                                        )
                               )
                        );

INSTANTIATE_TEST_CASE_P(tensor3, storage,
                        Values(TestData(d233a("A", "ddd"),
                                        {
                                          {
                                            // Dense index
                                          },
                                          {
                                            // Dense index
                                          },
                                          {
                                            // Dense index
                                          }
                                        },
                                        {1, 2, 0,
                                         0, 0, 0,
                                         0, 0, 3,

                                         0, 4, 0,
                                         0, 0, 0,
                                         5, 0, 6}
                                        ),
                               TestData(d233a("A", "sdd"),
                                        {
                                          {
                                            // Sparse index
                                            {0,2},
                                            {0,1}
                                          },
                                          {
                                            // Dense index
                                          } ,
                                          {
                                            // Dense index
                                          }
                                        },
                                        {1, 2, 0,
                                         0, 0, 0,
                                         0, 0, 3,

                                         0, 4, 0,
                                         0, 0, 0,
                                         5, 0, 6}
                                        ),
                               TestData(d233a("A", "dsd"),
                                        {
                                          {
                                            // Dense index
                                          },
                                          {
                                            // Sparse index
                                            {0,2,4},
                                            {0,2,0,2}
                                          },
                                          {
                                            // Dense index
                                          }
                                        },
                                        {1, 2, 0,
                                         0, 0, 3,

                                         0, 4, 0,
                                         5, 0, 6}
                                        ),
                               TestData(d233a("A", "ssd"),
                                        {
                                          {
                                            // Sparse index
                                            {0,2},
                                            {0,1}
                                          },
                                          {
                                            // Sparse index
                                            {0,2,4},
                                            {0,2,0,2}
                                          },
                                          {
                                            // Dense index
                                          }
                                        },
                                        {1, 2, 0,
                                         0, 0, 3,

                                         0, 4, 0,
                                         5, 0, 6}
                                        ),
                               TestData(d233a("A", "dds"),
                                        {
                                          {
                                            // Dense index
                                          },
                                          {
                                            // Dense index
                                          },
                                          {
                                            // Sparse index
                                            {0,2,2,3,4,4,6},
                                            {0,1,2, 1,0,2}
                                          }
                                        },
                                        {1, 2, 3, 4, 5, 6}
                                        ),
                               TestData(d233a("A", "sds"),
                                        {
                                          {
                                            // Sparse index
                                            {0,2},
                                            {0,1}
                                          },
                                          {
                                            // Dense index
                                          },
                                          {
                                            // Sparse index
                                            {0,2,2,3,4,4,6},
                                            {0,1,2, 1,0,2}
                                          }
                                        },
                                        {1, 2, 3, 4, 5, 6}
                                        ),
                               TestData(d233a("A", "dss"),
                                        {
                                          {
                                            // Dense index
                                          },
                                          {
                                            // Sparse index
                                            {0,2,4},
                                            {0,2,0,2}
                                          },
                                          {
                                            // Sparse index
                                            {0,2,3,4,6},
                                            {0,1,2, 1,0,2}
                                          }
                                        },
                                        {1, 2, 3, 4, 5, 6}
                                        ),
                               TestData(d233a("A", "sss"),
                                        {
                                          {
                                            // Sparse index
                                            {0,2},
                                            {0,1}
                                          },
                                          {
                                            // Sparse index
                                            {0,2,4},
                                            {0,2,0,2}
                                          },
                                          {
                                            // Sparse index
                                            {0,2,3,4,6},
                                            {0,1,2, 1,0,2}
                                          }
                                        },
                                        {1, 2, 3, 4, 5, 6}
                                        )
                               )
                        );
