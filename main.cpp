#include <iostream>
#include <vector>
#include <memory>
#include <string>
#include <fstream>
#include <sstream>
#include <stdexcept>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <dlfcn.h>
#endif

using namespace std;

typedef unsigned char* (*EncryptBytesFunc)(
    const unsigned char*,
    size_t,
    const char*,
    size_t*
);

typedef unsigned char* (*DecryptBytesFunc)(
    const unsigned char*,
    size_t,
    const char*,
    size_t*
);

typedef void (*FreeBytesFunc)(unsigned char*);

string escapeField(const string& value) {
    string result;

    for (char ch : value) {
        if (ch == '\\') {
            result += "\\\\";
        } else if (ch == '|') {
            result += "\\p";
        } else if (ch == '\n') {
            result += "\\n";
        } else {
            result += ch;
        }
    }

    return result;
}

string unescapeField(const string& value) {
    string result;

    for (size_t i = 0; i < value.size(); i++) {
        if (value[i] == '\\' && i + 1 < value.size()) {
            char next = value[i + 1];

            if (next == '\\') {
                result += '\\';
            } else if (next == 'p') {
                result += '|';
            } else if (next == 'n') {
                result += '\n';
            } else {
                result += next;
            }

            i++;
        } else {
            result += value[i];
        }
    }

    return result;
}

vector<string> splitByPipe(const string& line) {
    vector<string> parts;
    string current;
    bool slash = false;

    for (char ch : line) {
        if (slash) {
            current += '\\';
            current += ch;
            slash = false;
        } else if (ch == '\\') {
            slash = true;
        } else if (ch == '|') {
            parts.push_back(current);
            current.clear();
        } else {
            current += ch;
        }
    }

    if (slash) {
        current += '\\';
    }

    parts.push_back(current);
    return parts;
}

class Line {
public:
    virtual void print() const = 0;
    virtual string serialize() const = 0;
    virtual unique_ptr<Line> clone() const = 0;
    virtual ~Line() = default;
};

class TextLine : public Line {
private:
    string text;

public:
    explicit TextLine(const string& textValue) : text(textValue) {}

    void print() const override {
        cout << "Text: " << text << endl;
    }

    string serialize() const override {
        return "TEXT|" + escapeField(text);
    }

    unique_ptr<Line> clone() const override {
        return make_unique<TextLine>(*this);
    }
};

class ChecklistLine : public Line {
private:
    string item;
    bool checked;

public:
    ChecklistLine(const string& itemValue, bool checkedValue)
        : item(itemValue), checked(checkedValue) {}

    void print() const override {
        cout << "[" << (checked ? "x" : " ") << "] " << item << endl;
    }

    string serialize() const override {
        return "CHECK|" + string(checked ? "1" : "0") + "|" + escapeField(item);
    }

    unique_ptr<Line> clone() const override {
        return make_unique<ChecklistLine>(*this);
    }
};

class ContactLine : public Line {
private:
    string name;
    string surname;
    string email;

public:
    ContactLine(const string& nameValue, const string& surnameValue, const string& emailValue)
        : name(nameValue), surname(surnameValue), email(emailValue) {}

    void print() const override {
        cout << "Contact: " << name << " " << surname << ", email: " << email << endl;
    }

    string serialize() const override {
        return "CONTACT|" + escapeField(name) + "|" + escapeField(surname) + "|" + escapeField(email);
    }

    unique_ptr<Line> clone() const override {
        return make_unique<ContactLine>(*this);
    }
};

unique_ptr<Line> deserializeLine(const string& row) {
    vector<string> parts = splitByPipe(row);

    if (parts.empty()) {
        throw runtime_error("Empty row");
    }

    if (parts[0] == "TEXT" && parts.size() == 2) {
        return make_unique<TextLine>(unescapeField(parts[1]));
    }

    if (parts[0] == "CHECK" && parts.size() == 3) {
        return make_unique<ChecklistLine>(unescapeField(parts[2]), parts[1] == "1");
    }

    if (parts[0] == "CONTACT" && parts.size() == 4) {
        return make_unique<ContactLine>(
            unescapeField(parts[1]),
            unescapeField(parts[2]),
            unescapeField(parts[3])
        );
    }

    throw runtime_error("Unknown line format: " + row);
}

class Text {
private:
    vector<unique_ptr<Line>> lines;

public:
    Text() = default;

    Text(const Text& other) {
        for (const auto& line : other.lines) {
            lines.push_back(line->clone());
        }
    }

    Text& operator=(const Text& other) {
        if (this == &other) {
            return *this;
        }

        lines.clear();

        for (const auto& line : other.lines) {
            lines.push_back(line->clone());
        }

        return *this;
    }

    void addLine(unique_ptr<Line> line) {
        lines.push_back(std::move(line));
    }

    void removeLine(size_t index) {
        if (index >= lines.size()) {
            cout << "Invalid line index" << endl;
            return;
        }

        lines.erase(lines.begin() + index);
    }

    void print() const {
        if (lines.empty()) {
            cout << "Text is empty" << endl;
            return;
        }

        for (size_t i = 0; i < lines.size(); i++) {
            cout << i << ". ";
            lines[i]->print();
        }
    }

    string serialize() const {
        string result;

        for (const auto& line : lines) {
            result += line->serialize();
            result += "\n";
        }

        return result;
    }

    void deserialize(const string& data) {
        lines.clear();

        istringstream input(data);
        string row;

        while (getline(input, row)) {
            if (!row.empty()) {
                lines.push_back(deserializeLine(row));
            }
        }
    }
};

class History {
private:
    vector<Text> undoStack;
    vector<Text> redoStack;
    static const int LIMIT = 3;

public:
    void saveUndo(const Text& text) {
        if (undoStack.size() == LIMIT) {
            undoStack.erase(undoStack.begin());
        }

        undoStack.push_back(text);
        redoStack.clear();
    }

    bool undo(Text& text) {
        if (undoStack.empty()) {
            return false;
        }

        redoStack.push_back(text);
        text = undoStack.back();
        undoStack.pop_back();

        return true;
    }

    bool redo(Text& text) {
        if (redoStack.empty()) {
            return false;
        }

        undoStack.push_back(text);
        text = redoStack.back();
        redoStack.pop_back();

        return true;
    }
};

class Tab {
private:
    string name;
    Text text;
    History history;

public:
    explicit Tab(const string& tabName = "Tab") : name(tabName) {}

    string getName() const {
        return name;
    }

    Text& getText() {
        return text;
    }

    const Text& getText() const {
        return text;
    }

    void saveState() {
        history.saveUndo(text);
    }

    void undo() {
        if (history.undo(text)) {
            cout << "Undo completed" << endl;
        } else {
            cout << "Nothing to undo" << endl;
        }
    }

    void redo() {
        if (history.redo(text)) {
            cout << "Redo completed" << endl;
        } else {
            cout << "Nothing to redo" << endl;
        }
    }

    string serialize() const {
        return "TAB|" + escapeField(name) + "\n" + text.serialize() + "ENDTAB\n";
    }

    void deserialize(const string& tabName, const string& textData) {
        name = tabName;
        text.deserialize(textData);
    }
};

class TextEditor {
private:
    vector<Tab> tabs;
    size_t activeTabIndex;

public:
    TextEditor() : activeTabIndex(0) {
        tabs.emplace_back("Main");
    }

    Tab& activeTab() {
        return tabs[activeTabIndex];
    }

    const Tab& activeTab() const {
        return tabs[activeTabIndex];
    }

    void createTab(const string& name) {
        tabs.emplace_back(name);
        activeTabIndex = tabs.size() - 1;
        cout << "Tab created and selected" << endl;
    }

    void switchTab(size_t index) {
        if (index >= tabs.size()) {
            cout << "Invalid tab index" << endl;
            return;
        }

        activeTabIndex = index;
        cout << "Active tab changed" << endl;
    }

    void listTabs() const {
        for (size_t i = 0; i < tabs.size(); i++) {
            cout << i << ". " << tabs[i].getName();

            if (i == activeTabIndex) {
                cout << " <- active";
            }

            cout << endl;
        }
    }

    string serialize() const {
        string result;

        for (const auto& tab : tabs) {
            result += tab.serialize();
        }

        return result;
    }

    void deserialize(const string& data) {
        tabs.clear();
        activeTabIndex = 0;

        istringstream input(data);
        string row;
        string currentTabName;
        string currentTextData;
        bool insideTab = false;

        while (getline(input, row)) {
            if (row.rfind("TAB|", 0) == 0) {
                currentTabName = unescapeField(row.substr(4));
                currentTextData.clear();
                insideTab = true;
            } else if (row == "ENDTAB") {
                Tab tab;
                tab.deserialize(currentTabName, currentTextData);
                tabs.push_back(tab);
                insideTab = false;
            } else if (insideTab) {
                currentTextData += row + "\n";
            }
        }

        if (tabs.empty()) {
            tabs.emplace_back("Main");
        }
    }
};

class FileManager {
public:
    static void saveTextFile(const string& path, const string& data) {
        ofstream file(path, ios::binary);

        if (!file) {
            throw runtime_error("Cannot open file for saving");
        }

        file.write(data.data(), static_cast<streamsize>(data.size()));
    }

    static string loadTextFile(const string& path) {
        ifstream file(path, ios::binary);

        if (!file) {
            throw runtime_error("Cannot open file for loading");
        }

        stringstream buffer;
        buffer << file.rdbuf();

        return buffer.str();
    }

    static vector<unsigned char> loadBytes(const string& path) {
        ifstream file(path, ios::binary);

        if (!file) {
            throw runtime_error("Cannot open input file");
        }

        return vector<unsigned char>(
            istreambuf_iterator<char>(file),
            istreambuf_iterator<char>()
        );
    }

    static void saveBytes(const string& path, const vector<unsigned char>& data) {
        ofstream file(path, ios::binary);

        if (!file) {
            throw runtime_error("Cannot open output file");
        }

        file.write(
            reinterpret_cast<const char*>(data.data()),
            static_cast<streamsize>(data.size())
        );
    }
};

class Cipher {
private:
#ifdef _WIN32
    HMODULE library;
#else
    void* library;
#endif

    EncryptBytesFunc encryptFunc;
    DecryptBytesFunc decryptFunc;
    FreeBytesFunc freeFunc;

    void openLibrary() {
#ifdef _WIN32
        library = LoadLibraryA("cipher.dll");
#else
        library = dlopen("./libcipher.dylib", RTLD_LAZY);

        if (!library) {
            library = dlopen("./libcipher.so", RTLD_LAZY);
        }
#endif

        if (!library) {
            throw runtime_error("Cannot load cipher library");
        }

#ifdef _WIN32
        encryptFunc = reinterpret_cast<EncryptBytesFunc>(
            GetProcAddress(library, "cipher_encrypt_bytes")
        );
        decryptFunc = reinterpret_cast<DecryptBytesFunc>(
            GetProcAddress(library, "cipher_decrypt_bytes")
        );
        freeFunc = reinterpret_cast<FreeBytesFunc>(
            GetProcAddress(library, "cipher_free_bytes")
        );
#else
        encryptFunc = reinterpret_cast<EncryptBytesFunc>(
            dlsym(library, "cipher_encrypt_bytes")
        );
        decryptFunc = reinterpret_cast<DecryptBytesFunc>(
            dlsym(library, "cipher_decrypt_bytes")
        );
        freeFunc = reinterpret_cast<FreeBytesFunc>(
            dlsym(library, "cipher_free_bytes")
        );
#endif

        if (!encryptFunc || !decryptFunc || !freeFunc) {
            closeLibrary();
            throw runtime_error("Cannot load cipher functions");
        }
    }

    void closeLibrary() {
        if (!library) {
            return;
        }

#ifdef _WIN32
        FreeLibrary(library);
#else
        dlclose(library);
#endif

        library = nullptr;
    }

public:
    Cipher() : library(nullptr), encryptFunc(nullptr), decryptFunc(nullptr), freeFunc(nullptr) {
        openLibrary();
    }

    ~Cipher() {
        closeLibrary();
    }

    vector<unsigned char> encrypt(const vector<unsigned char>& data, const string& key) {
        size_t outSize = 0;

        unsigned char* rawResult = encryptFunc(
            data.data(),
            data.size(),
            key.c_str(),
            &outSize
        );

        if (!rawResult) {
            throw runtime_error("Encryption failed");
        }

        vector<unsigned char> result(rawResult, rawResult + outSize);
        freeFunc(rawResult);

        return result;
    }

    vector<unsigned char> decrypt(const vector<unsigned char>& data, const string& key) {
        size_t outSize = 0;

        unsigned char* rawResult = decryptFunc(
            data.data(),
            data.size(),
            key.c_str(),
            &outSize
        );

        if (!rawResult) {
            throw runtime_error("Decryption failed");
        }

        vector<unsigned char> result(rawResult, rawResult + outSize);
        freeFunc(rawResult);

        return result;
    }
};

class CommandLineInterface {
private:
    TextEditor editor;

    string readLine(const string& message) {
        cout << message;
        string value;
        getline(cin, value);
        return value;
    }

    int readInt(const string& message) {
        cout << message;
        int value;
        cin >> value;
        cin.ignore();
        return value;
    }

    void addTextLine() {
        string text = readLine("Enter text: ");

        editor.activeTab().saveState();
        editor.activeTab().getText().addLine(make_unique<TextLine>(text));

        cout << "Text line added" << endl;
    }

    void addChecklistLine() {
        string item = readLine("Enter checklist item: ");
        int checked = readInt("Checked? 1 - yes, 0 - no: ");

        editor.activeTab().saveState();
        editor.activeTab().getText().addLine(make_unique<ChecklistLine>(item, checked == 1));

        cout << "Checklist line added" << endl;
    }

    void addContactLine() {
        string name = readLine("Enter name: ");
        string surname = readLine("Enter surname: ");
        string email = readLine("Enter email: ");

        editor.activeTab().saveState();
        editor.activeTab().getText().addLine(make_unique<ContactLine>(name, surname, email));

        cout << "Contact line added" << endl;
    }

    void removeLine() {
        int index = readInt("Enter line index: ");

        editor.activeTab().saveState();
        editor.activeTab().getText().removeLine(static_cast<size_t>(index));
    }

    void savePlain() {
        string path = readLine("Enter file path: ");
        FileManager::saveTextFile(path, editor.serialize());

        cout << "Saved" << endl;
    }

    void loadPlain() {
        string path = readLine("Enter file path: ");
        string data = FileManager::loadTextFile(path);

        editor.deserialize(data);

        cout << "Loaded" << endl;
    }

    void encryptToFile() {
        string path = readLine("Enter encrypted output file path: ");
        string key = readLine("Enter key: ");

        string data = editor.serialize();

        vector<unsigned char> bytes(data.begin(), data.end());

        Cipher cipher;
        vector<unsigned char> encrypted = cipher.encrypt(bytes, key);

        FileManager::saveBytes(path, encrypted);

        cout << "Encrypted text saved" << endl;
    }

    void loadEncryptedFile() {
        string path = readLine("Enter encrypted input file path: ");
        string key = readLine("Enter key: ");

        vector<unsigned char> encrypted = FileManager::loadBytes(path);

        Cipher cipher;
        vector<unsigned char> decrypted = cipher.decrypt(encrypted, key);

        string data(decrypted.begin(), decrypted.end());
        editor.deserialize(data);

        cout << "Encrypted text loaded and decrypted" << endl;
    }

    void createTab() {
        string name = readLine("Enter tab name: ");
        editor.createTab(name);
    }

    void switchTab() {
        int index = readInt("Enter tab index: ");
        editor.switchTab(static_cast<size_t>(index));
    }

    void printMenu() const {
        cout << "\nChoose command:" << endl;
        cout << "1. Add text line" << endl;
        cout << "2. Add checklist line" << endl;
        cout << "3. Add contact line" << endl;
        cout << "4. Print active tab" << endl;
        cout << "5. Remove line from active tab" << endl;
        cout << "6. Undo" << endl;
        cout << "7. Redo" << endl;
        cout << "8. Save plain file" << endl;
        cout << "9. Load plain file" << endl;
        cout << "10. Encrypt whole editor and save" << endl;
        cout << "11. Load encrypted file and decrypt" << endl;
        cout << "12. Create new tab" << endl;
        cout << "13. Switch tab" << endl;
        cout << "14. List tabs" << endl;
        cout << "0. Exit" << endl;
        cout << "> ";
    }

public:
    void run() {
        while (true) {
            try {
                printMenu();

                int command;
                cin >> command;
                cin.ignore();

                switch (command) {
                    case 1:
                        addTextLine();
                        break;
                    case 2:
                        addChecklistLine();
                        break;
                    case 3:
                        addContactLine();
                        break;
                    case 4:
                        editor.activeTab().getText().print();
                        break;
                    case 5:
                        removeLine();
                        break;
                    case 6:
                        editor.activeTab().undo();
                        break;
                    case 7:
                        editor.activeTab().redo();
                        break;
                    case 8:
                        savePlain();
                        break;
                    case 9:
                        loadPlain();
                        break;
                    case 10:
                        encryptToFile();
                        break;
                    case 11:
                        loadEncryptedFile();
                        break;
                    case 12:
                        createTab();
                        break;
                    case 13:
                        switchTab();
                        break;
                    case 14:
                        editor.listTabs();
                        break;
                    case 0:
                        cout << "Program finished" << endl;
                        return;
                    default:
                        cout << "Unknown command" << endl;
                }
            } catch (const exception& error) {
                cout << "Error: " << error.what() << endl;
            }
        }
    }
};

int main() {
    CommandLineInterface cli;
    cli.run();

    return 0;
}