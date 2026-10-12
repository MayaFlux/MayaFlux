#pragma once

#include <cstdlib>
#include <optional>
#include <string>

namespace MayaFlux::Test {

class ScopedEnv {
public:
    ScopedEnv(const char* name, const char* value)
        : m_name(name)
    {
        if (const char* previous = std::getenv(name)) {
            m_previous = previous;
        }
        assign(value);
    }

    ~ScopedEnv()
    {
        assign(m_previous ? m_previous->c_str() : nullptr);
    }

    ScopedEnv(const ScopedEnv&) = delete;
    ScopedEnv& operator=(const ScopedEnv&) = delete;
    ScopedEnv(ScopedEnv&&) = delete;
    ScopedEnv& operator=(ScopedEnv&&) = delete;

private:
    void assign(const char* value) const
    {
#if defined(_WIN32)
        _putenv_s(m_name.c_str(), value ? value : "");
#else
        if (value) {
            setenv(m_name.c_str(), value, 1);
        } else {
            unsetenv(m_name.c_str());
        }
#endif
    }

    std::string m_name;
    std::optional<std::string> m_previous;
};

}
