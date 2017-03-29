#include "type_util.h"
#include <string>
#include <typeinfo>
#include "cell.h"
#include "gtest/gtest.h"

namespace bdm {

TEST(TypeTernaryOperatorTest, True) {
  type_ternary_operator<true, int, double>::type data;
  EXPECT_EQ(std::string("i"), typeid(data).name());
}

TEST(TypeTernaryOperatorTest, False) {
  type_ternary_operator<false, int, double>::type data;
  EXPECT_EQ(std::string("d"), typeid(data).name());
}

TEST(is_std_arrayTest, True) {
  using type = is_std_array<std::array<double, 3>>;
  EXPECT_TRUE(type::value);
}

TEST(is_std_arrayTest, FalseForVcVectorBackendSimdArray) {
  EXPECT_FALSE(is_std_array<VcVectorBackend::SimdArray<double>>::value);
}

TEST(is_std_arrayTest, False) {
  EXPECT_FALSE(is_std_array<std::vector<double>>::value);
}

template <typename TBackend>
struct Widget {
  using Backend = TBackend;
};

TEST(is_scalarTest, All) {
  EXPECT_FALSE(is_scalar<Widget<VcVectorBackend>>::value);
  EXPECT_TRUE(is_scalar<Widget<ScalarBackend>>::value);
}

TEST(is_soaTest, All) {
  EXPECT_FALSE(is_soa<VcVectorBackend>::value);
  EXPECT_FALSE(is_soa<ScalarBackend>::value);
  EXPECT_TRUE(is_soa<VcSoaBackend>::value);
  EXPECT_TRUE(is_soa<VcSoaRefBackend>::value);
}

}  // namespace bdm
