#include <bytestream.hpp>
#include <iostream>


int main() {
    std::byte arr[100];
    OutByteStream s(&arr[0], sizeof(arr));
    s << 17;

    InByteStream ss(&arr[0], sizeof(arr));
    int a;
    ss >> a;

    std::cout << a << '\n';

}
