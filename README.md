# c-openhab-test-suite-backend

Stateless C/C++ HTTP backend for the
[c-openhab-test-suite](https://github.com/Michdo93/c-openhab-test-suite)
web frontend.

The tester functions are pure C11. The HTTP layer is C++17 (cpp-httplib,
header-only) with `extern "C"` includes for the C library. `std::cout` and
`std::cerr` are redirected during each call so diagnostic messages are returned
as `"output"` in the response.

## Endpoints

| Method | Path | Description |
|---|---|---|
| `GET` | `/` | Health check / wake-up |
| `POST` | `/api/connect` | Verify credentials → `{ loggedIn, isCloud }` |
| `POST` | `/api/test` | Run a tester function → `{ result, output }` |

### `POST /api/test`

```json
{
  "url":      "https://myopenhab.org",
  "username": "user@example.com",
  "password": "secret",
  "tester":   "ItemTester",
  "method":   "testSwitch",
  "params":   ["MySwitch", "ON", "ON", 10]
}
```

Available testers: `ItemTester`, `ThingTester`, `RuleTester`,
`ChannelTester`, `PersistenceTester`, `SitemapTester`.

## Git submodules

```
extern/
├── c-openhab-rest-client/   ← https://github.com/Michdo93/c-openhab-rest-client
└── c-openhab-test-suite/    ← https://github.com/Michdo93/c-openhab-test-suite
```

Clone with submodules:

```bash
git clone --recurse-submodules \
  https://github.com/Michdo93/c-openhab-test-suite-backend.git
```

## Local build

```bash
sudo apt-get install -y libcurl4-openssl-dev cmake build-essential
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release && cmake --build . -- -j$(nproc)
PORT=8080 ./openhab_test_suite_backend
```

## Deploy on Render.com

1. Push `c-openhab-rest-client` and `c-openhab-test-suite` to GitHub.
2. Push this repo with submodules:
   ```bash
   git submodule add https://github.com/Michdo93/c-openhab-rest-client.git \
       extern/c-openhab-rest-client
   git submodule add https://github.com/Michdo93/c-openhab-test-suite.git \
       extern/c-openhab-test-suite
   git add . && git commit -m "add submodules"
   git push origin main
   ```
3. **Render.com → New → Web Service → Docker → Frankfurt → Free → PORT=8080 → Deploy**.

Live URL: `https://c-openhab-test-suite-backend.onrender.com`

## License

MIT
