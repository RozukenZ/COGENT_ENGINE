
#include <iostream>
#include <vector>

const unsigned char arr[] = { 0x12, 0x34 };
const unsigned int arr_size = 2;

int main() {
    std::vector<char> v(arr, arr + arr_size);
    std::cout << v.size() << std::endl;
    return 0;
}
