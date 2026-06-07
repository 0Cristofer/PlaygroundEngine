#include <meta>

import std;

struct Vec3
{
    float x, y, z;
};

struct Transform
{
    Vec3 position;
    Vec3 rotation;
    float scale;
};

template <typename T>
void PrintMembers(const T& obj)
{
    std::cout << "Type: " << std::meta::identifier_of(^^T) << "\n";
    
    template for (constexpr auto member :
        std::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::current())))
    {
        std::cout << "member: " << std::meta::identifier_of(member) << ", value: " << obj.[:member:] << "\n";
    }
}

int main()
{
    Vec3 vec3 = {1, 2, 3};
    Vec3 vec31 = {2, 3, 5};
    // Transform transform = {vec3, vec31, 2};
    PrintMembers(vec3);
    PrintMembers(vec31);
    // PrintMembers<Transform>(transform);
    return 0;
}
