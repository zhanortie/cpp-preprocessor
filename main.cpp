#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

using namespace std;
using filesystem::path;

path operator""_p(const char* data, std::size_t sz) {
    return path(data, data + sz);
}

//рекурсивная функция для обработки файла 
bool FileProcess(istream& in, ostream& out, const path& current_file, const vector<path>& include_directories) {

    static const regex re_quotes(R"inc(\s*#\s*include\s*"([^"]*)"\s*.*)inc"); //регулярное выражение для поиска с кавычками
    static const regex re_brackets(R"inc(\s*#\s*include\s*<([^>]*)>\s*.*)inc"); //регулярное выражение для поиска с угловыми скобками

    string line;
    int line_number = 0;

    // цикл чтения файла по строкам
    while (getline(in, line)) {
        ++line_number;

        //проверка на include с кавычками или угловыми скобками
        smatch m;
        const bool is_quotes = regex_match(line, m, re_quotes);
        const bool is_brackets = !is_quotes && regex_match(line, m, re_brackets);

        // если обычная строка - продолжаем и копируем ее
        if (!is_quotes && !is_brackets) {
            out << line << '\n';
            continue;
        }

        // если include, то берем имя файла 
        path inc_path = string(m[1]);

        // поиск и открытие файла из include 
        ifstream inc_stream;
        path found_path; // переменная для запоминания пути файла 
        bool file_found = false;

        // если файл в кавычках и если он открылся, запоминаем его путь
        if (is_quotes) {
            path local_path = current_file.parent_path() / inc_path;
            inc_stream.open(local_path);
            if (inc_stream.is_open()) {
                found_path = local_path;
                file_found = true;
            }
        }

        //если файл не найден, то ищем по директориям
        if (!file_found) {
            for (const auto& dir : include_directories) {
                path candidate = dir / inc_path;
                inc_stream.close();
                inc_stream.open(candidate);

                if (inc_stream.is_open()) {
                    found_path = candidate;
                    file_found = true;
                    break;
                }
            }
        }

        //вывод ошибки, если не найден файл
        if (!file_found) {
            cout << "unknown include file " << inc_path.string()
                << " at file " << current_file.string()
                << " at line " << line_number << endl;
            return false;
        }

        // вместо строки с include подставляем новый файл рекурсией
        if (!FileProcess(inc_stream, out, found_path, include_directories)) {
            return false;
        }
    }

    return true;
}

bool Preprocess(const path& in_file, const path& out_file, const vector<path>& include_directories) {
    ifstream input(in_file);
    if (!input.is_open()) {
        return false; //если не удалось открыть начальный файл
    }

    ofstream output(out_file); // создание, перезаписывание выходного файла
    // проверка выходного файла 
    if (!output.is_open()) {
        return false;
    }

    return FileProcess(input, output, in_file, include_directories);
}

string GetFileContents(string file) {
    ifstream stream(file);

    // конструируем string по двум итераторам
    return { 
        (istreambuf_iterator<char>(stream)), istreambuf_iterator<char>() 
    };
}

void Test() {
    error_code err;
    filesystem::remove_all("sources"_p, err);
    filesystem::create_directories("sources"_p / "include2"_p / "lib"_p, err);
    filesystem::create_directories("sources"_p / "include1"_p, err);
    filesystem::create_directories("sources"_p / "dir1"_p / "subdir"_p, err);

    {
        ofstream file("sources/a.cpp");
        file << "// this comment before include\n"
            "#include \"dir1/b.h\"\n"
            "// text between b.h and c.h\n"
            "#include \"dir1/d.h\"\n"
            "\n"
            "int SayHello() {\n"
            "    cout << \"hello, world!\" << endl;\n"
            "#   include<dummy.txt>\n"
            "}\n"s;
    }
    {
        ofstream file("sources/dir1/b.h");
        file << "// text from b.h before include\n"
            "#include \"subdir/c.h\"\n"
            "// text from b.h after include"s;
    }
    {
        ofstream file("sources/dir1/subdir/c.h");
        file << "// text from c.h before include\n"
            "#include <std1.h>\n"
            "// text from c.h after include\n"s;
    }
    {
        ofstream file("sources/dir1/d.h");
        file << "// text from d.h before include\n"
            "#include \"lib/std2.h\"\n"
            "// text from d.h after include\n"s;
    }
    {
        ofstream file("sources/include1/std1.h");
        file << "// std1\n"s;
    }
    {
        ofstream file("sources/include2/lib/std2.h");
        file << "// std2\n"s;
    }

    assert((!Preprocess("sources"_p / "a.cpp"_p, "sources"_p / "a.in"_p,
        { "sources"_p / "include1"_p,"sources"_p / "include2"_p })));

    ostringstream test_out;
    test_out << "// this comment before include\n"
        "// text from b.h before include\n"
        "// text from c.h before include\n"
        "// std1\n"
        "// text from c.h after include\n"
        "// text from b.h after include\n"
        "// text between b.h and c.h\n"
        "// text from d.h before include\n"
        "// std2\n"
        "// text from d.h after include\n"
        "\n"
        "int SayHello() {\n"
        "    cout << \"hello, world!\" << endl;\n"s;

    assert(GetFileContents("sources/a.in"s) == test_out.str());
}

int main() {
    Test();
}