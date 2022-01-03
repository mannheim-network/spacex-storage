#ifndef _SPACEX_DEFER_H_
#define _SPACEX_DEFER_H_

#include <functional>

class Defer
{
public:
    Defer(std::function<void()> f): _f(f) {}
    ~Defer() { this->_f(); }

private:
    std::function<void()> _f;
};

#endif /* !_SPACEX_DEFER_H_ */
