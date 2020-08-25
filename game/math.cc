/* math.cc - This module implements fundamental constructs for the kind of 
 * mathematics most commonly used in computer graphics, such as matrices,
 * vectors and transformations. */
import <ints>;

export module math;
export namespace math
{
	/* A value that can be added, subtracted, multiplied and divided by itself. */
	template<typename T>
	concept number = require(T a, T b)
	{
		{ 0 } -> T; { 1 } -> T;
		{ a + b } -> T; { b + a } -> T;
		{ a - b } -> T; { b - a } -> T;
		{ a * b } -> T; { b * a } -> T;
		{ a / b } -> T; { b / a } -> T;
	};

	/* A 4x4 square matrix with generic data type. */
	template<number T>
	class mat4
	{
	protected:
		/* The underlying data. Stored as a row-major array. */
		T data[16];
	public:
		/* Creates an identity matrix. */
		static mat4<T> identity() const noexcept 
		{
			mat4<T> mat;
			mat.data[0x0] = 1; mat.data[0x1] = 0; mat.data[0x2] = 0; mat.data[0x3] = 0;
			mat.data[0x4] = 0; mat.data[0x5] = 1; mat.data[0x6] = 0; mat.data[0x7] = 0;
			mat.data[0x8] = 0; mat.data[0x9] = 0; mat.data[0xa] = 1; mat.data[0xb] = 0;
			mat.data[0xc] = 0; mat.data[0xd] = 0; mat.data[0xe] = 0; mat.data[0xf] = 1;

			return mat;
		}

		/* Creates a tranlstion matrix in three dimensions. */
		static mat4<T> translate(T x, T y, T z) const noexcept
		{
			mat4<T> mat;
			mat.data[0x0] = 1; mat.data[0x1] = 0; mat.data[0x2] = 0; mat.data[0x3] = x;
			mat.data[0x4] = 0; mat.data[0x5] = 1; mat.data[0x6] = 0; mat.data[0x7] = y;
			mat.data[0x8] = 0; mat.data[0x9] = 0; mat.data[0xa] = 1; mat.data[0xb] = z;
			mat.data[0xc] = 0; mat.data[0xd] = 0; mat.data[0xe] = 0; mat.data[0xf] = 1;

			return mat;
		}

		/* Creates a scaling matrix in three dimensions. */
		static mat4<T> scale(T x, T y, T z) const noexcept
		{
			mat4<T> mat;
			mat.data[0x0] = x; mat.data[0x1] = 0; mat.data[0x2] = 0; mat.data[0x3] = 0;
			mat.data[0x4] = 0; mat.data[0x5] = y; mat.data[0x6] = 0; mat.data[0x7] = 0;
			mat.data[0x8] = 0; mat.data[0x9] = 0; mat.data[0xa] = z; mat.data[0xb] = 0;
			mat.data[0xc] = 0; mat.data[0xd] = 0; mat.data[0xe] = 0; mat.data[0xf] = 1;

			return mat;
		}

		/* Initializes a new matrix with the value of zero. */
		mat4() noexcept
		{
			for(uint8_t i = 0; i < 16; ++i)
				this->data[i] = 0;
		}

		mat4<T> operator *(mat4<T> const& rhs) const noexcept
		{
			matrix<T, side> target;
			for(uint8_t i = 0; i < 4; ++i)
				for(uint8_t j = 0; j < 4; ++j)
				{
					
				}

			return target;
		}

		mat4<T> operator +(mat4<T> const& rhs) const noexcept
		{
			matrix<T, side> target;
			for(uint8_t i = 0; i < side; ++i)
				for(uint8_t j = 0; j < side; ++j)
					target.data[i * side + j] = this->data[i * side + j] + rhs.data[i * side + j];
		}
	};

	/* A 4-dimensional vector. */
	template<number T, uint8_t size = 4>
	class vec4 : public matrix<T, size>
	{
	public:
		vector() noexcept : matrix() { }
		vector(T x, T y, T z, T w) noexcept : matrix()
		{
			this.data[0x0] = x;
			this.data[0x4] = y;
			this.data[0x8] = z;
			this.data[0xc] = w;
		}

		T x() const noexcept { return this.data[0x0]; }
		T y() const noexcept { return this.data[0x4]; }
		T z() const noexcept { return this.data[0x8]; }
		T w() const noexcept { return this.data[0xc]; }
	};
}

