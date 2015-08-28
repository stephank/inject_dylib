## inject_dylib

Inject_dylib (indy) allows you to load a dynamic library and call a function in
another process. Indy currently supports targetting 32-bit and 64-bit x86
processes, interoperates with the sandbox, and has been tested on OS X 10.10.

```C
#include "indy.h"

// In your injector process:
int main(int argc, char **argv)
{
    const char *str = "Hello world!";

    struct indy_result res;
    indy_inject(&(struct indy_info) {
        .pid = 1234,
        .dylib_path = "/path/to/libfoo.dylib",
        .dylib_entry_symbol = "foo_entry",
        .user_data = (const void *) str,
        .user_data_size = strlen(str) + 1
    }, &res);
    if (res.error != NULL) {
        fprintf(stderr, res.error, res.os_value);
        return EXIT_FAILURE;
    }

    return res.exit_status;
}

// And in your injected dynamic library:
uint32_t foo_entry(void *user_data)
{
    printf("%s\n", (char *) user_data);
    return EXIT_SUCCESS;
}
```

### Security restrictions

For your application to be able to do code injection, it needs to be code
signed by a certificate in the system keychain. Either create a self-signed
certificate in the system keychain and reboot, or sign up for the Apple
Developer program to get an Apple signed certificate. The latter is probably a
requirement if you wish to distribute your application.

In addition, add the following key to your application `Info.plist`:

```XML
<key>SecTaskAccess</key>
<array>
    <string>allowed</string>
</array>
```

Alternatively, you can use a privileged helper. Either way, it is probably wise
to restrict this kind of access to a separate helper executable within your
application bundle.

The injector process currently cannot have App Sandbox enabled.

### Technical details

Indy is inspired by [mach_inject], but takes a very different approach,
abstracts away more details, and adds support for different target achitectures
and the app sandbox.

Indy copies a small architecture specific loader into the target process, and
starts a pthread. The loader does roughly the following:

```C
extern struct indy_info info;
extern char *dylib_token;
extern uint32_t exit_status;

void loader()
{
    sandbox_extension_consume(dylib_token);
    void *handle = dlopen(info.dylib_path, RTLD_LOCAL);
    if (handle != NULL) {
        indy_entry entry = dlsym(handle, info.dylib_entry_symbol);
        if (entry != NULL)
            exit_status = entry(info.user_data);
        dlclose(handle);
    }
}
```

Indy hides much of the intricate detail from the user. This listing excludes
architecture specific setup. Before the above code runs, a proper pthread is
created. By using regular dynamic loader calls, the library is free to pull
in other library dependencies.

The loader code, info structure and runtime data are all allocated in a single
block. This block is automatically deallocated when the code finishes.

To find the various system functions in the target process address space, indy
looks for the dynamic loader first, and uses its debugger interface to iterate
libraries. Indy then parses Mach-O to locate the actual functions and creates a
table for the loader to reference. The functions used in the loader are all
libSystem functions, which are currently guaranteed to be linked into every
process on OS X.

 [mach_inject]: https://github.com/rentzsch/mach_inject/

### Release notes

##### v0.2.0 - 2015-07-28

Rework of API:

 - Most significantly, `indy_inject` is now a blocking call that
   doesn't exit until the injected code does. This simplifies cleanup a lot. 
 - An exit status is returned back to the injector.
 - The Mach communication port was removed.
 - The entry point now directly takes user data.

##### v0.1.0 - 2015-07-22

Initial release.

### License

Copyright (c) 2015 Stéphan Kochen

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
