#include "zeno/unreal/UnrealTool.h"

std::string zeno::remote::RandomString2(size_t Length) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 61);
    const char charset[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
    std::string str(Length,0);
    std::generate_n( str.begin(), Length, [&](){ return charset[ dis(gen) ]; } );
    return str;
}

std::string zeno::remote::RandomString(size_t Length) {
    auto randchar = []() -> char
    {
      const char charset[] =
          "0123456789"
          "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
          "abcdefghijklmnopqrstuvwxyz";
      const size_t max_index = (sizeof(charset) - 1);
      // Use C++ 11 random
      return charset[ rand() % max_index ];
    };
    std::string str(Length,0);
    std::generate_n( str.begin(), Length, randchar );
    return str;
}

std::string zeno::remote::Flags::GetCurrentSession() {
    if (IsMainProcess()) {
        std::string Result = CurrentSession;
        return Result;
    } else if (!CurrentSession.empty()) {
        return CurrentSession;
    } else {
        // Request session key from main process
        httplib::Client Cli { ZENO_TOOL_SERVER_ADDRESS };
        auto Res = Cli.Get("/session/current", httplib::Headers { { ZENO_SESSION_HEADER_KEY, ZENO_LOCAL_TOKEN } });
        if (Res && Res->status == 200) {
            CurrentSession = Res->body;
            return Res->body;
        } else {
            return "";
        }
    }
}
bool zeno::remote::Flags::IsMainProcess() const {
    return IsMainProcess_;
}

void zeno::remote::Flags::SetIsMainProcess(bool isMainProcess) {
    IsMainProcess_ = isMainProcess;
}
zeno::remote::Flags::Flags() : IsMainProcess_(false)
{}

void zeno::remote::SubjectRegistry::Push(const std::vector<SubjectContainer> &InitList, const std::string &SessionKey) {
    if (StaticFlags.IsMainProcess()) {
        std::set<std::string> ChangeList;
        auto& ElementMap = GetOrCreateSessionElement(SessionKey);

        for (const SubjectContainer& Value : InitList) {
            ChangeList.emplace(Value.Name);
            ElementMap.try_emplace(Value.Name, Value);
        }
        if (Callback) {
            Callback(ChangeList, SessionKey);
        }
    } else {
        // In child process, transfer data with http
        httplib::Client Cli { ZENO_TOOL_SERVER_ADDRESS };
        Cli.set_default_headers({ {ZENO_SESSION_HEADER_KEY, ZENO_LOCAL_TOKEN} });
        SubjectContainerList List { InitList };
        std::vector<uint8_t> Data = msgpack::pack(List);
        const std::string& Url = StringFormat("/subject/push?session_key=%s", SessionKey);
        Cli.Post(Url, reinterpret_cast<const char*>(Data.data()), Data.size(), "application/binary");
    }
}

void zeno::remote::SubjectRegistry::SetParameter(const std::string &SessionKey, const std::string &Key,
                                                 zeno::remote::ParamValue &Value) {
    if (StaticFlags.IsMainProcess()) {
        auto& ParamMapIter = GetOrCreate(SessionalParameters, SessionKey);

        ParamMapIter.insert(std::make_pair(Key, Value));
    } else {
        // In child process, transfer data with http
        httplib::Client Cli { ZENO_TOOL_SERVER_ADDRESS };
        Cli.set_default_headers({ {ZENO_SESSION_HEADER_KEY, ZENO_LOCAL_TOKEN} });
        httplib::Params Param;
        Param.insert(std::make_pair("session_key", SessionKey));
        Param.insert(std::make_pair("key", Key));
        std::vector<uint8_t> Data = msgpack::pack(Value);
        const httplib::Result Response = Cli.Post("/graph/param/push", reinterpret_cast<const char*>(Data.data()), Data.size(), "application/binary");
    }
}

const zeno::remote::ParamValue *zeno::remote::SubjectRegistry::GetParameter(const std::string &SessionKey,
                                                                            const std::string &Key) const {
    static zeno::remote::ParamValue TempValue;
    if (StaticFlags.IsMainProcess()) {
        auto ParamMapIter = SessionalParameters.find(SessionKey);
        if (ParamMapIter != SessionalParameters.end()) {
            auto ParamIter = ParamMapIter->second.find(Key);
            if (ParamIter != ParamMapIter->second.end()) {
                TempValue = ParamIter->second;
                return &TempValue;
            }
        }
        return nullptr;
    } else {
        // In child process, transfer data with http
        httplib::Client Cli{ZENO_TOOL_SERVER_ADDRESS};
        Cli.set_default_headers({{ZENO_SESSION_HEADER_KEY, ZENO_LOCAL_TOKEN}});
        httplib::Params Param;
        Param.insert(std::make_pair("key", Key));
        const httplib::Result Response =
            Cli.Get("/graph/param/fetch", Param, httplib::Headers{}, httplib::Progress{});
        if (Response) {
            const std::string &Body = Response->body;
            std::error_code Err;
            auto List = msgpack::unpack<struct ParamValueBatch>(
                reinterpret_cast<uint8_t *>(const_cast<char *>(Body.data())), Body.size(), Err);
            if (!Err) {
                for (const auto &Subject : List.Values) {
                    // Alloc new memory and copy it
                    // TODO [darc] : fix race condition(might be) :
                    TempValue = Subject;
                    return &TempValue;
                }
            }
        }
        return nullptr;
    }
}
