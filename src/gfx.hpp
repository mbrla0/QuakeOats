/* gfx.cc - The rasterizer. This module provides access to a triangle
 * renderer with a multi-stage pipeline and other graphics utilities. */
#pragma once
//module;
#include "thread_utils.hpp"	/* Thanks Natan. */

//export module gfx;

//import <string>;	/* For standard string types.		*/
//import <sstream>;	/* AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA.	*/
//import <cstdint>;	/* For standard integer types.		*/
//import <stdexcept>;	/* For standard exception types.	*/
//import <concepts>;	/* For standard concepts.			*/
//import str;			/* Haha UTF-8 go brr.				*/
#include <string>
#include <sstream>
#include <cstdint>
#include <stdexcept>

/*export*/ namespace gfx
{
    /** RGBA 32 color values. */
    struct alignas(4) PixelRgba32
    {
        uint8_t red;
        uint8_t green;
        uint8_t blue;
        uint8_t alpha;

        PixelRgba32()
        {
            this->red   = 0;
            this->green = 0;
            this->blue  = 0;
            this->alpha = 0xff;
        }
    };

    /* A plane of data points. */
    template<typename T>
    class Plane
    {
    protected:
        /* Extent of the plane, in pixels. */
        uint32_t _width, _height;

        /* Color storage. */
        T *_data;
    protected:
        /* Checks for bounds on input X and Y coordinates, thowing a range error
         * in case the coordinates land somewhere outside the plane. */
        void _check_bounds(uint32_t x, uint32_t y) const
        {
            if(x >= this->_width || y >= this->_height)
            {
                std::stringstream what;
                what << u8"Could not access pixel at ";
                what << u8"(" << x << u8", " << y << u8")";
                what << u8", expected < ";
                what << u8"(" << _width << u8", " << _height << u8")";

                throw std::range_error(what.str());
            }
        }
    public:
        /* Creates a new plane with the given dimensions.
         *
         * The backing storage is allocated immediately and left unitialized.
         * Therefore, the data inside of a plane at creation is undefined, which
         * should not be a problem, as most buffer operations are write-only,
         * and thus performing a buffer clear would be unneeded.
         *
         * If a uniform, known value is desired the user should call `clear()`
         * in order to initialize the contents of the plane.
         */
        Plane(uint32_t width, uint32_t height)
                : _width(width), _height(height)
        {
            this->_data = new T[width * height];
        }

        ~Plane()
        {
            delete[] this->_data;
        }

        /* Acquire a reference to a data point in the plane at the given x and y
         * coordinate index, not checking for boundaries.
         *
         * # Safety
         * This does not perform any boundary checks on the  input coordinates
         * and is thus wildly unsafe. Prefer `at()` over this unless it is
         * guaranteed that you'll only pass valid coordinates.
         */
        constexpr const T& at_unchecked(uint32_t x, uint32_t y) const noexcept
        {
            return this->_data[y * this->_width + x];
        }

        /* Acquire a reference to a data point in the plane at the given x and y
         * coordinate index, not checking for boundaries.
         *
         * # Safety
         * This does not perform any boundary checks on the  input coordinates
         * and is thus wildly unsafe. Prefer `at()` over this unless it is
         * guaranteed that you'll only pass valid coordinates.
         */
        constexpr T& at_unchecked(uint32_t x, uint32_t y) noexcept
        {
            return this->_data[y * this->_width + x];
        }

        /* Acquire a reference to a data point in the plane at the given x and y
         * coordinate index, checking for boundaries.
         *
         * # Exceptions
         * Throws an instance of `std::range_error` if the given x or y values
         * land outside the range of the plane.
         */
        const T& at(uint32_t x, uint32_t y) const
        {
            this->_check_bounds(x, y);
            return this->_data[y * this->_width + x];
        }

        /* Acquire a reference to a data point in the plane at the given x and y
         * coordinate index, checking for boundaries.
         *
         * # Exceptions
         * Throws an instance of `std::range_error` if the given x or y values
         * land outside the range of the plane.
         */
        T& at(uint32_t x, uint32_t y)
        {
            this->_check_bounds(x, y);
            return this->_data[y * this->_width + x];
        }

        /* Clears the buffer with a given data value. */
        void clear(T clear)
        {
            for(uint32_t i = 0; i < this->_height; ++i)
                for(uint32_t j = 0; j < this->_width; ++j)
                    this->_data[i * this->_width + j] = clear;
        }

        /* Gets the width of this plane. */
        uint32_t width() const
        {
            return this->_width;
        }

        /* Gets the height of this plane. */
        uint32_t height() const
        {
            return this->_height;
        }

        /* Returns the backing storage for this plane. The elements in this
         * storage are guaranteed to be laid out in row-major with no stride
         * from one row to the next.
         *
         * # Safety
         * None. I hope you know what you're doing.
         */
        T* data()
        {
            return this->_data;
        }
    };

//	template<typename T, typename P>
//	concept Slope = requires(T slope, double dPos, float fPos)
//	{
//		{ slope.at(dPos) } -> std::same_as<P>;
//		{ slope.at(fPos) } -> std::same_as<P>;
//	};

    template<typename P, typename S>
//		requires Slope<S, P>
    class Raster
    {
    public:
        /* Given a point, this function applies a transformation to it,
         * returning the resulting transformed point.
         *
         * # Synchronization
         * This is expected to behave as a pure function, thus, no
         * synchronization guarantees are made about its invocations.
         */
        std::function<P(P)> transform;

        /* Given a point, returns a tuple with the coordinates of this point
         * in screen space.
         *
         * # Synchronization
         * Same as transform.
         */
        std::function<std::tuple<uint32_t, uint32_t>(P)> screen;

        /* Given a pair of points, create a slope that goes from one to the
         * first to the second point.
         *
         * # Synchronization
         * Same as transform.
         */
        std::function<S(P, P)> slope;

        /* This function gets called at the bottom of the raster pipeline and
         * to it is given a position X in screen space, followed by a position
         * Y, also in screen space, followed by an interpollated coordinate for
         * the current pixel in the space resulting from the previous
         * transformation.
         *
         * # Interpolation
         * The interpolation is done by linearly interpolating each dimension of
         * the point independently.
         *
         * # Synchronization
         * It is guaranteed that no two simultaneous invocations of this
         * function will have the same values for X and Y. These two values can,
         * therefore, be used as, for example, indices to individual pixels
         * without the need for external synchronization.
         */
        std::function<void(uint32_t, uint32_t, P)> painter;
    protected:
        /* Triangle work bundle. */
        struct Triangle
        {
            P point0;
            P point1;
            P point2;
        };

        /* Render worker and its related parameters. */
        struct Worker
        {
            /* Handle to the backing thread this worker runs on. */
            std::thread thread;

            /* Area value quantifying the amount of work current being
             * shouldered by this worker. */
            std::atomic<size_t> area;

            /* Queue of triangles to be rendered by this worker. */
            work_queue<Triangle> queue;
        };

        /* When work is submitted to this raster, it gets parallelized in such
         * a way that tries to balance the work being done between all threads.
         *
         * At creation, this vector should be populated with one worker for
         * every hardware thread provided by the host.
         */
        std::vector<Worker> workers;

        /* This is the thread pool that will be running the rasterization tasks
         * on each and every one of our submitted triangles. */
        thread_pool pool;
    protected:

        /* Actually perform the raster operation using the given triangle. */
        void rasterize(Triangle t)  {
            auto [x0, y0, x1, y1, x2, y2] = std::tuple_cat(
                    this->screen(t.point0),
                    this->screen(t.point1),
                    this->screen(t.point2));
            P a, b, c;

            a = t.point0;
            b = t.point1;
            c = t.point2;

            a = this->transform(a);
            b = this->transform(b);
            c = this->transform(c);

            /* Sort the points primarily by increasing Y and, secondy, by increasing X. */
            if(std::tie(y0, x0) > std::tie(y1, x1)) { std::swap(a, b); std::swap(y0, y1); std::swap(x0, x1); }
            if(std::tie(y1, x1) > std::tie(y2, x2)) { std::swap(b, c); std::swap(y1, y2); std::swap(x1, x2); }
            if(std::tie(y0, x0) > std::tie(y1, x1)) { std::swap(a, b); std::swap(y0, y1); std::swap(x0, x1); }

            /* Side of the shortest slope. */
            bool shortside = (y1 - y0) * (x2 - x0) < (x1 - x0) * (y2 - y0);

            S slopes[2] =
                    {
                            /* Yes, the initializer list is required here because otherwise
                             * C++ would complain it needs S() to be valid. */
                            shortside == 0 ? this->slope(a, b) : this->slope(a, c),
                            shortside == 1 ? this->slope(a, b) : this->slope(a, c)
                    };

            auto ye = y1;
            for(auto y = y0;; ++y)
            {
                if(y >= ye)
                {
                    if(ye == y2)
                        /* We've reached the end of the second bend. */
                        break;

                    /* We're at the end of the first bend, change the slopes. */
                    ye = y2;
                    slopes[shortside] = this->slope(b, c);
                }

                double posY = (y - y0) / (y2 - y0);
                auto p0 = slopes[0].at(posY);
                auto p1 = slopes[1].at(posY);

                auto x0 = std::get<0>(this->screen(p0));
                auto x1 = std::get<0>(this->screen(p1));
                if(x0 > x1) { std::swap(x0, x1); std::swap(p0, p1); }

                S slope = this->slope(p0, p1);
                for(auto x = x0; x0 < x1; ++x)
                {
                    double posX = (x - x0) / (x1 - x0);
                    auto p = slope.at(posX);

                    /* In order for this operation to be safe, it has to be
                     * guaranteed that no other triangle operation is currently
                     * modifying this one pixel value.
                     *
                     * TODO: Actually implement the synchronization here. */
                    this->painter(x, y, p);
                }
            }
        }

    public:
        Raster()
        /* Create the thread pool.
         *
         * This will create a new unbalanced thread pool (which should not
         * be a problem, given it's work stealing) with as many workers as
         * there are hardware threads. */
                : pool(thread_pool::default_concurrency())
        {
            /* Since C++ gets really bloated and hard to read at the mildest of
             * things and, no doubt, to generate a compile time error for the
             * uninitialized functions it would turn this code unto an
             * unreadable piece of satire.
             *
             * And strings have to be encoded in the input encodingn because
             * C++20 saw fit to effectively destroy all interop with UTF-8
             * strings by making `char8_t` its own special snowflake type, thus
             * rendering UTF-8 support in C++20 a fucking joke.
             * GOD I HATE THIS LANGUAGE SO MUCH
             *
             * std::function by default will throw `bad_function_call` when
             * trying to invoke a default-initialized function. I find that to
             * be still a bit too unclear for my taste, so I'll just do custom
             * runtime exception messages for this. */
            this->transform = [](P) -> P
            {
                auto what = u8"raster call missing transform function";
                throw std::runtime_error(what);
            };
            this->screen = [](P) -> std::tuple<uint32_t, uint32_t>
            {
                auto what = u8"raster call missing screen space fucntion";
                throw std::runtime_error(what);
            };
            this->slope = [](P, P) -> S
            {
                auto what = u8"raster call missing slope creator function";
                throw std::runtime_error(what);
            };
            this->painter = [](auto, auto, auto)
            {
                auto what = u8"raster call missing painter function";
                throw std::runtime_error(what);
            };
        }

        /* Dispatches the rendering of a triangle, given the coordinates for its
         * three vertices. This function returns a future that will be complete
         * when the triangle has been completely drawn. */
        std::future<void> dispatch(P p0, P p1, P p2)
        {
            auto task = make_task<void>([&]()
            {
                Triangle triangle =
                        {
                                .point0 = p0,
                                .point1 = p1,
                                .point2 = p2
                        };

                this->rasterize(triangle);
            });
            return this->pool.submit_task(task);
        }
    };
}
