/* str.cc - Potentially unsafe conversion from UTF-8 literals to string literals
 * because I think my sanity is more important than defined behavior outside of 
 * x86.
 *
 * This module brings back usability to UTF-8 literals in C++ by adding a
 * postposition string operator such that you may convert UTF-8 character
 * literals to an std::string.
 *
 * This is done by simply casting the const pointer to `char8_t` to a const
 * pointer of `char`, which shouldnt really be *undefined behaviour* so long
 * as there are no special alignment requirements for your char type.
 *
 * Also "fb" stands for "Fuck Bjarne".
 */

export module str;
import <string>; /* For cursed strings. */

export const std::string operator ""_fb (const char8_t *c, size_t)
{
	auto d = (const char*) c;
	return std::string(d);
}
