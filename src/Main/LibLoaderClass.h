#pragma once

#include "LibLoader.h"
#include <string>
#include <iostream>
#include <vector>

class iLibraryFunction {
protected:
    friend class LibraryLoader;
    void virtual Load(DynamicLibrary* lib) = 0;
};

template <typename tReturnType, typename... ParameterTypes>
class LibraryFunction : public iLibraryFunction {
public:
    LibraryFunction(std::string_view function_name)
        : function_name { function_name }
    {
    }
    tReturnType operator()(ParameterTypes... args) const
    {

        return func_ptr_(args...);
    };

protected:
    std::string_view function_name {};
    tReturnType (*func_ptr_)(ParameterTypes...) = nullptr;

    void Load(DynamicLibrary* lib) override
    {
        func_ptr_ = (tReturnType(*)(ParameterTypes...))GetDynamicLibraryFunction(lib, function_name.data());
        assert(func_ptr_ && "[LibraryFunction]: Failed to get address of function in loaded library");
    }
};

class LibraryLoader : public DynamicLibrary {
public:
    using FunctionList = std::vector<iLibraryFunction*>;

    LibraryLoader(std::string_view library_name, std::string_view library_path, FunctionList loadableFunctions)
        : functions { loadableFunctions }
    {
        InitDynamicLibrary(this, library_name.data(), library_path.data());
    }

    ~LibraryLoader()
    {
        CloseDynamicLibrary(this);
    }

    bool TryReload()
    {
        bool lib_reload = LoadDynamicLibrary(this);

        if (lib_reload) {
            for (auto& func : functions) {
                func->Load(this);
            }
        }

        return lib_reload;
    };

private:
    FunctionList functions;
};
