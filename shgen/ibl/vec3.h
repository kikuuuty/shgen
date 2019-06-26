#ifndef VEC3_H__
#define VEC3_H__

namespace ibl {
namespace math {

    template <typename T>
    struct TVec3
    {
        using value_type = typename T;
        using reference = typename T&;
        using const_reference = typename const T&;
        using size_type = size_t;
        static constexpr size_t SIZE = 3;

        union {
            T v[SIZE];
            //TVec2<T> xy;
            //TVec2<T> st;
            //TVec2<T> rg;
            struct {
                union {
                    T x;
                    T s;
                    T r;
                };
                union {
                    struct { T y, z; };
                    struct { T t, p; };
                    struct { T g, b; };
                    //TVec2<T> yz;
                    //TVec2<T> tp;
                    //TVec2<T> gb;
                };
            };
        };

        inline constexpr size_type size() const { return SIZE; }

        // array access
        inline constexpr T const& operator[](size_t i) const {
            // only possible in C++0x14 with constexpr
            assert(i < SIZE);
            return v[i];
        }

        inline constexpr T& operator[](size_t i) {
            assert(i < SIZE);
            return v[i];
        }

        // constructors

        // default constructor
        constexpr TVec3() = default;

        // handles implicit conversion to a tvec4. must not be explicit.
        template<typename A>
        constexpr TVec3(A v)
            : x(static_cast<T>(v))
            , y(static_cast<T>(v))
            , z(static_cast<T>(v)) {}

        template<typename A, typename B, typename C>
        constexpr TVec3(A x, B y, C z)
            : x(static_cast<T>(x))
            , y(static_cast<T>(y))
            , z(static_cast<T>(z)) {}

        //template<typename A, typename B>
        //constexpr TVec3(const TVec2<A>& v, B z) : x(v.x), y(v.y), z(z) {}

        template<typename A>
        constexpr TVec3(const TVec3<A>& v)
            : x(static_cast<T>(v.x))
            , y(static_cast<T>(v.y))
            , z(static_cast<T>(v.z)) {}

        TVec3& operator+=(const TVec3& v)
        {
            x += v.x;
            y += v.y;
            z += v.z;
            return *this;
        }

        template <typename U>
        TVec3& operator*=(U v)
        {
            x *= v;
            y *= v;
            z *= v;
            return *this;
        }
    };

    template <typename T, typename = typename std::enable_if<std::is_arithmetic<T>::value>::type>
    using vec3 = TVec3<T>;

    template <typename T>
    vec3<T> operator*(const vec3<T>& a, T b)
    {
        return {a.x * b, a.y * b, a.z * b};
    }

    template <typename T, typename U>
    vec3<T> operator*(const vec3<T>& a, U b)
    {
        return {a.x * b, a.y * b, a.z * b};
    }

    using float3 = vec3<float>;
    using double3 = vec3<double>;
}
}

#endif
