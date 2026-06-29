/**
 * c-openhab-test-suite-backend
 * ─────────────────────────────
 * C++ HTTP server (cpp-httplib) that wraps the C test-suite library.
 * All tester functions are C, the HTTP plumbing is C++.
 *
 * Endpoints
 * ─────────
 *   GET  /             → health / wake-up
 *   POST /api/connect  → verify credentials → { loggedIn, isCloud }
 *   POST /api/test     → run tester method  → { result, output }
 */

#define OPENHAB_STATIC

#include "httplib.h"
#include <nlohmann/json.hpp>

extern "C" {
#include <openhab/openhab.h>
#include <openhab/openhab_testsuite.h>
}

#include <cstdlib>
#include <cstring>
#include <string>
#include <sstream>
#include <iostream>
#include <stdexcept>
#include <unistd.h>

using json = nlohmann::json;

// ─── CORS ─────────────────────────────────────────────────────────────────────

static void setCORS(httplib::Response& res) {
    res.set_header("Access-Control-Allow-Origin",  "*");
    res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
}

// ─── Response helpers ─────────────────────────────────────────────────────────

static void respond_ok(httplib::Response& res, const json& j) {
    setCORS(res);
    res.set_content(j.dump(), "application/json");
}
static void respond_err(httplib::Response& res, const std::string& msg, int code = 400) {
    setCORS(res);
    res.status = code;
    res.set_content(json{{"error", msg}}.dump(), "application/json");
}

// ─── Client factory ───────────────────────────────────────────────────────────

static openhab_client_t* makeClient(const json& body) {
    std::string url  = body.value("url",      "");
    std::string user = body.value("username", "");
    std::string pass = body.value("password", "");
    std::string tok  = body.value("token",    "");
    if (url.empty()) throw std::invalid_argument("url is required");
    while (!url.empty() && url.back()=='/') url.pop_back();
    return openhab_client_create(
        url.c_str(),
        user.empty() ? nullptr : user.c_str(),
        pass.empty() ? nullptr : pass.c_str(),
        tok.empty()  ? nullptr : tok.c_str());
}

// ─── Output capture ───────────────────────────────────────────────────────────

struct Capture {
    std::ostringstream buf;
    std::streambuf*    oldOut;
    std::streambuf*    oldErr;
    Capture() {
        oldOut = std::cout.rdbuf(buf.rdbuf());
        oldErr = std::cerr.rdbuf(buf.rdbuf());
    }
    ~Capture() {
        std::cout.rdbuf(oldOut);
        std::cerr.rdbuf(oldErr);
    }
    std::string str() const { return buf.str(); }
};

// ─── Parameter helpers ────────────────────────────────────────────────────────

static std::string sp(const json& p, int i, const std::string& def="") {
    if (i<(int)p.size()&&!p[i].is_null())
        return p[i].is_string() ? p[i].get<std::string>() : p[i].dump();
    return def;
}
static int ip(const json& p, int i, int def=10) {
    if (i<(int)p.size()) {
        if (p[i].is_number()) return p[i].get<int>();
        try { return std::stoi(sp(p,i)); } catch(...) {}
    }
    return def;
}

// ─── Dispatch ─────────────────────────────────────────────────────────────────

static json dispatch(openhab_client_t* c,
                     const std::string& tester,
                     const std::string& method,
                     const json& p) {
    json result = false;
    std::string output;

    auto run_bool = [&](auto fn) {
        Capture cap;
        result = (fn() != 0);
        output = cap.str();
    };
    auto run_str = [&](auto fn) {
        Capture cap;
        char* r = fn();
        result  = r ? json(r) : json(nullptr);
        if (r) free(r);
        output  = cap.str();
    };

    if (tester == "ItemTester") {
        if      (method=="doesItemExist")
            run_bool([&]{ return openhab_item_tester_does_item_exist(c,sp(p,0).c_str()); });
        else if (method=="checkItemIsType")
            run_bool([&]{ return openhab_item_tester_check_item_is_type(c,sp(p,0).c_str(),sp(p,1).c_str()); });
        else if (method=="checkItemHasState")
            run_bool([&]{ return openhab_item_tester_check_item_has_state(c,sp(p,0).c_str(),sp(p,1).c_str()); });
        else if (method=="isGroupItem")
            run_bool([&]{ return openhab_item_tester_is_group_item(c,sp(p,0).c_str()); });
        else if (method=="doesGroupContainMember")
            run_bool([&]{ return openhab_item_tester_does_group_contain_member(c,sp(p,0).c_str(),sp(p,1).c_str()); });
        else if (method=="checkGroupMemberState")
            run_bool([&]{ return openhab_item_tester_check_group_member_state(c,sp(p,0).c_str(),sp(p,1).c_str(),sp(p,2).c_str()); });
        else if (method=="getGroupMembers")
            run_str([&]{ return openhab_item_tester_get_group_members(c,sp(p,0).c_str()); });
        else if (method=="testSwitch")
            run_bool([&]{ return openhab_item_tester_test_switch(c,sp(p,0).c_str(),sp(p,1).c_str(),sp(p,2).c_str(),ip(p,3)); });
        else if (method=="testContact")
            run_bool([&]{ return openhab_item_tester_test_contact(c,sp(p,0).c_str(),sp(p,1).c_str(),sp(p,2).c_str(),ip(p,3)); });
        else if (method=="testColor")
            run_bool([&]{ return openhab_item_tester_test_color(c,sp(p,0).c_str(),sp(p,1).c_str(),sp(p,2).c_str(),ip(p,3)); });
        else if (method=="testDimmer")
            run_bool([&]{ return openhab_item_tester_test_dimmer(c,sp(p,0).c_str(),sp(p,1).c_str(),sp(p,2).c_str(),ip(p,3)); });
        else if (method=="testRollershutter")
            run_bool([&]{ return openhab_item_tester_test_rollershutter(c,sp(p,0).c_str(),sp(p,1).c_str(),sp(p,2).c_str(),ip(p,3)); });
        else if (method=="testNumber")
            run_bool([&]{ return openhab_item_tester_test_number(c,sp(p,0).c_str(),sp(p,1).c_str(),sp(p,2).c_str(),ip(p,3)); });
        else if (method=="testPlayer")
            run_bool([&]{ return openhab_item_tester_test_player(c,sp(p,0).c_str(),sp(p,1).c_str(),sp(p,2).c_str(),ip(p,3)); });
        else if (method=="testDateTime")
            run_bool([&]{ return openhab_item_tester_test_datetime(c,sp(p,0).c_str(),sp(p,1).c_str(),sp(p,2).c_str(),ip(p,3)); });
        else if (method=="testLocation")
            run_bool([&]{ return openhab_item_tester_test_location(c,sp(p,0).c_str(),sp(p,1).c_str(),sp(p,2).c_str(),ip(p,3)); });
        else if (method=="testImage")
            run_bool([&]{ return openhab_item_tester_test_image(c,sp(p,0).c_str(),sp(p,1).c_str(),sp(p,2).c_str(),ip(p,3)); });
        else if (method=="testString")
            run_bool([&]{ return openhab_item_tester_test_string(c,sp(p,0).c_str(),sp(p,1).c_str(),sp(p,2).c_str(),ip(p,3)); });
        else throw std::invalid_argument("Unknown ItemTester method: " + method);
    }
    else if (tester == "ThingTester") {
        if      (method=="getThingStatus")
            run_str([&]{ return openhab_thing_tester_get_thing_status(c,sp(p,0).c_str()); });
        else if (method=="isThingStatus")
            run_bool([&]{ return openhab_thing_tester_is_thing_status(c,sp(p,0).c_str(),sp(p,1).c_str()); });
        else if (method=="isThingOnline")
            run_bool([&]{ return openhab_thing_tester_is_thing_online(c,sp(p,0).c_str()); });
        else if (method=="isThingOffline")
            run_bool([&]{ return openhab_thing_tester_is_thing_offline(c,sp(p,0).c_str()); });
        else if (method=="isThingPending")
            run_bool([&]{ return openhab_thing_tester_is_thing_pending(c,sp(p,0).c_str()); });
        else if (method=="isThingUnknown")
            run_bool([&]{ return openhab_thing_tester_is_thing_unknown(c,sp(p,0).c_str()); });
        else if (method=="isThingUninitialized")
            run_bool([&]{ return openhab_thing_tester_is_thing_uninitialized(c,sp(p,0).c_str()); });
        else if (method=="isThingError")
            run_bool([&]{ return openhab_thing_tester_is_thing_error(c,sp(p,0).c_str()); });
        else if (method=="enableThing")
            run_bool([&]{ return openhab_thing_tester_enable_thing(c,sp(p,0).c_str()); });
        else if (method=="disableThing")
            run_bool([&]{ return openhab_thing_tester_disable_thing(c,sp(p,0).c_str()); });
        else throw std::invalid_argument("Unknown ThingTester method: " + method);
    }
    else if (tester == "RuleTester") {
        if      (method=="getRuleStatus")
            run_str([&]{ return openhab_rule_tester_get_rule_status(c,sp(p,0).c_str()); });
        else if (method=="isRuleActive")
            run_bool([&]{ return openhab_rule_tester_is_rule_active(c,sp(p,0).c_str()); });
        else if (method=="isRuleDisabled")
            run_bool([&]{ return openhab_rule_tester_is_rule_disabled(c,sp(p,0).c_str()); });
        else if (method=="isRuleRunning")
            run_bool([&]{ return openhab_rule_tester_is_rule_running(c,sp(p,0).c_str()); });
        else if (method=="isRuleIdle")
            run_bool([&]{ return openhab_rule_tester_is_rule_idle(c,sp(p,0).c_str()); });
        else if (method=="enableRule")
            run_bool([&]{ return openhab_rule_tester_enable_rule(c,sp(p,0).c_str()); });
        else if (method=="disableRule")
            run_bool([&]{ return openhab_rule_tester_disable_rule(c,sp(p,0).c_str()); });
        else if (method=="runRule")
            run_bool([&]{ return openhab_rule_tester_run_rule(c,sp(p,0).c_str(),nullptr); });
        else if (method=="testRuleExecution")
            run_bool([&]{ return openhab_rule_tester_test_rule_execution(c,sp(p,0).c_str(),sp(p,1).c_str(),sp(p,2).c_str()); });
        else throw std::invalid_argument("Unknown RuleTester method: " + method);
    }
    else if (tester == "ChannelTester") {
        if      (method=="isItemLinkedToChannel")
            run_bool([&]{ return openhab_channel_tester_is_item_linked_to_channel(c,sp(p,0).c_str(),sp(p,1).c_str()); });
        else if (method=="getLinksForItem")
            run_str([&]{ return openhab_channel_tester_get_links_for_item(c,sp(p,0).c_str()); });
        else if (method=="isItemLinkedToAnyChannel")
            run_bool([&]{ return openhab_channel_tester_is_item_linked_to_any_channel(c,sp(p,0).c_str()); });
        else if (method=="hasOrphanedLinks")
            run_bool([&]{ return openhab_channel_tester_has_orphaned_links(c); });
        else throw std::invalid_argument("Unknown ChannelTester method: " + method);
    }
    else if (tester == "PersistenceTester") {
        if      (method=="isItemPersisted")
            run_bool([&]{ return openhab_persistence_tester_is_item_persisted(c,sp(p,0).c_str(),sp(p,1).c_str()); });
        else if (method=="hasDataInRange")
            run_bool([&]{ return openhab_persistence_tester_has_data_in_range(c,sp(p,0).c_str(),sp(p,1).c_str(),sp(p,2).c_str(),sp(p,3).c_str()); });
        else if (method=="checkLastPersistedState")
            run_bool([&]{ return openhab_persistence_tester_check_last_persisted_state(c,sp(p,0).c_str(),sp(p,1).c_str(),sp(p,2).c_str()); });
        else throw std::invalid_argument("Unknown PersistenceTester method: " + method);
    }
    else if (tester == "SitemapTester") {
        if      (method=="doesSitemapExist")
            run_bool([&]{ return openhab_sitemap_tester_does_sitemap_exist(c,sp(p,0).c_str()); });
        else if (method=="doesSitemapContainItem")
            run_bool([&]{ return openhab_sitemap_tester_does_sitemap_contain_item(c,sp(p,0).c_str(),sp(p,1).c_str()); });
        else throw std::invalid_argument("Unknown SitemapTester method: " + method);
    }
    else {
        throw std::invalid_argument(
            "Unknown tester '" + tester + "'. Valid: ItemTester, ThingTester, "
            "RuleTester, ChannelTester, PersistenceTester, SitemapTester");
    }

    // Trim trailing whitespace from output
    while (!output.empty() && (output.back()=='\n'||output.back()=='\r'||output.back()==' '))
        output.pop_back();

    return json{{"result", result}, {"output", output}};
}

// ─── Main ─────────────────────────────────────────────────────────────────────

int main() {
    const char* portEnv = std::getenv("PORT");
    int port = portEnv ? std::stoi(portEnv) : 8080;

    httplib::Server svr;

    // OPTIONS preflight
    svr.Options("/(.*)", [](const httplib::Request&, httplib::Response& res) {
        setCORS(res); res.status = 204;
    });

    // Health check / wake-up
    svr.Get("/", [](const httplib::Request&, httplib::Response& res) {
        respond_ok(res, {{"status","ok"},{"service","c-openhab-test-suite-backend"}});
    });

    // POST /api/connect
    svr.Post("/api/connect", [](const httplib::Request& req, httplib::Response& res) {
        json body;
        try { body = json::parse(req.body); }
        catch (...) { respond_err(res,"Invalid JSON body"); return; }

        openhab_client_t* c = nullptr;
        try { c = makeClient(body); }
        catch (const std::exception& e) {
            respond_ok(res,{{"loggedIn",false},{"isCloud",false},{"error",e.what()}});
            return;
        }
        respond_ok(res,{{"loggedIn",(bool)openhab_client_is_logged_in(c)},
                        {"isCloud", (bool)openhab_client_is_cloud(c)}});
        openhab_client_destroy(c);
    });

    // POST /api/test
    svr.Post("/api/test", [](const httplib::Request& req, httplib::Response& res) {
        json body;
        try { body = json::parse(req.body); }
        catch (...) { respond_err(res,"Invalid JSON body"); return; }

        std::string tester = body.value("tester","");
        std::string method = body.value("method","");
        json params        = body.value("params", json::array());

        if (tester.empty()) { respond_err(res,"tester is required"); return; }
        if (method.empty()) { respond_err(res,"method is required"); return; }

        openhab_client_t* c = nullptr;
        try { c = makeClient(body); }
        catch (const std::exception& e) {
            respond_err(res, std::string("Connection config error: ")+e.what()); return;
        }
        if (!openhab_client_is_logged_in(c)) {
            openhab_client_destroy(c);
            respond_err(res,"Could not connect to openHAB — check credentials", 401); return;
        }

        json result;
        try {
            result = dispatch(c, tester, method, params);
        } catch (const std::invalid_argument& e) {
            openhab_client_destroy(c);
            respond_err(res, e.what(), 400); return;
        } catch (const std::exception& e) {
            openhab_client_destroy(c);
            respond_err(res, e.what(), 500); return;
        }

        openhab_client_destroy(c);
        respond_ok(res, result);
    });

    std::cout << "c-openhab-test-suite-backend running on port " << port << "\n";
    svr.listen("0.0.0.0", port);
    return 0;
}
