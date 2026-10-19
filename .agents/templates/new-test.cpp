// Template: New Test Case
// Usage: python3 .agents/scripts/add-test.py --name test_my_feature --file tests/test_my_feature.cpp

#include "test.hpp"

#include "wilfred/{header}"

void {name}() {
    using namespace wilfred;

    // Arrange
    {arrange}

    // Act
    {act}

    // Assert
    {assert}
}