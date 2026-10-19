// Template: New Mini Command
// 1. Add to MiniKind enum in include/wilfred/search/minis.hpp
// 2. Add parser hook in src/search/minis.cpp parse_mini_intent()
// 3. Add switch case in mini_cards()

// In MiniKind enum:
/*
enum class MiniKind {
    ...
    MyCmd,  // <-- ADD HERE
};
*/

// In parse_mini_intent():
/*
else if (key == "mycmd") {
    set(MiniKind::MyCmd, rest.empty());
}
*/

// In mini_cards() switch:
/*
case MiniKind::MyCmd:
    out.push_back(card(
        "My Cmd",           // Title
        "Description",      // Subtitle
        "mycmd_payload",    // Payload (copied on Enter)
        "mycmd",            // Kind label
        10000,              // Score
        ResultAction::Copy, // Action on Enter
        -1                  // Meter (-1 = none)
    ));
    break;
*/

// Help text in src/locale/locale.cpp kEnglish[]:
/*
{"help.XX", "mycmd [args]  \u00b7  description"},
*/

// Localization key in src/locale/locale.cpp:
/*
{"mini.mycmd.title", "My Cmd"},
{"mini.mycmd.sub", "Description \u00b7 enter copies"},
*/