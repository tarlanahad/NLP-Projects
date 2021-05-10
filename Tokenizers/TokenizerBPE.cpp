/* 
 * Copyright (C) Tarlan Ahadli - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Written by Tarlan Ahadli <tarlanahad@gmail.com>, May 2021
 */

#include <wchar.h>
#include <stdlib.h>
#include <locale.h>
#include <string>
#include <iostream>
#include <vector>
#include <unordered_map>
#include <regex>
#include <fstream>
#include <algorithm>
#include <math.h>

using namespace std;

const wstring END_TOKEN = L"•";
const wstring UNK_TOKEN = L"<UNK>";

class Utils
{
public:
    static vector<wstring> split(wstring str)
    {
        vector<wstring> splitted;

        wstring s = L"";

        for (int j = 0; j < str.length(); j++)
        {
            wchar_t c = str[j];
            if (str[j] == L' ')
            {
                splitted.push_back(s);
                s = L"";
            }
            else
                s += c;
        }

        return splitted;
    }

    static wchar_t filter_char(wchar_t c)
    {
        return c;
        if ((c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') ||
            (wstring(L"əƏüÜğĞÖöıIşŞçÇ ").find(c) != wstring::npos))
            return c;
        if (wstring(L"-\n").find(c) != wstring::npos)
        {
            return L' ';
        }
        return L'\0';
    }

    static wstring replace_all(wstring str, wstring search, wstring replace)
    {
        for (int i = str.find(search); i >= 0; i = str.find(search))
            str.replace(i, search.size(), replace);
        return str;
    }

    static bool compare_len(const std::wstring &a, const std::wstring &b)
    {
        return (a.size() > b.size());
    }
};

class TokenizerBPE
{

public:
    vector<wstring> tokens;

    TokenizerBPE() {}

    TokenizerBPE(string file_path, int vocab_size)
    {
        this->vocab = build_vocab(&file_path[0]);
        this->vocab_size = vocab_size;
    }

    double calculate_perplexity(char *path)
    {

        char *locale = setlocale(LC_ALL, "");
        FILE *in = fopen(path, "r");

        wchar_t c;
        wstring txt = L"";

        while ((c = fgetwc(in)) != WEOF)
            txt += (c);

        fclose(in);

        cout << "Test Model Loaded" << endl;

        vector<wstring> ids = encode(txt, true);

        unordered_map<wstring, double> ids_counts;

        for (wstring s : ids)
            ids_counts[s] += 1.0;

        double N = ids.size();
        double s = 0;
        for (wstring id : ids)
        {
            s += log(1.0 / (ids_counts[id] / N));
        }

        return exp(s / N);
    }

    void fit()
    {
        tokens = get_tokens();

        int N = tokens.size();
        int i = 0;

        cout << "Before-BPE Token Size: " << N << endl;

        while (N < vocab_size)
        {
            i += 1;

            unordered_map<wstring, int> pair_stats = get_pair_stats();

            if (pair_stats.size() == 0)
                break;

            wstring best_pair = get_best_pair(pair_stats);

            vocab = merge_vocab(best_pair, vocab);

            tokens = get_tokens();
            N = tokens.size();

            wcout << "  Iter: " << i << " | Token size: " << N << endl;
        }
    }

    vector<wstring> encode(wstring str, bool return_ids)
    {

        wchar_t c;
        wstring n_str = L"";
        vector<wstring> vocab;

        for (int i = 0; i < str.length(); i++)
        {
            wchar_t c = str[i];
            c = Utils::filter_char(c);
            if (c == L' ' || i == (str.length() - 1))
            {
                if (i == (str.length() - 1))
                    n_str += (c);
                n_str += END_TOKEN;
                vocab.push_back(n_str);
                n_str = L"";
            }
            else
            {
                n_str += (c);
            }
        }

        vector<wstring> encoded_str;
        for (wstring str : vocab)
        {
            for (int i = 0; i < tokens.size(); i++)
            {
                wstring token = tokens[i];
                if (str.find(token) == 0)
                {
                    encoded_str.push_back(return_ids ? to_wstring(i) : token);
                    str = str.substr(token.length(), str.length());
                    i = 0;
                    if (str.length() == 0)
                        break;
                }

                if (str.length() != 0 && i == (tokens.size() - 1))
                    encoded_str.push_back(UNK_TOKEN);
            }
        }

        return encoded_str;
    }

    void save_model(string path)
    {

        std::ofstream myfile;

        myfile.open(path);

        for (int i = 0; i < tokens.size(); i++)
        {
            wstring x = tokens[i];
            for (wchar_t c : x)
                myfile << int(c) << " ";
            myfile << endl;
        }

        myfile.close();
    }

    void load_model(char *path)
    {

        char *locale = setlocale(LC_ALL, "");
        FILE *in = fopen(path, "r");

        wchar_t c;
        wstring token = L"";
        wstring word = L"";

        int b = 0;

        while ((c = fgetwc(in)) != WEOF)
        {
            if (c == L' ' || c == L'\n')
            {
                if (c == L'\n' && token != L"")
                {
                    tokens.push_back(token);
                    token = L"";
                }
                else
                {
                    token += wchar_t(stoi(word));
                    word = L"";
                }
            }
            else
                word += (c);
        }

        fclose(in);
    }

private:
    unordered_map<wstring, int> vocab;
    int vocab_size;

    /////// Functions /////////

    unordered_map<wstring, int> build_vocab(char *path)
    {

        char *locale = setlocale(LC_ALL, "");
        FILE *in = fopen(path, "r");

        wchar_t c;
        wstring word = L"";
        unordered_map<wstring, int> vocab;

        while ((c = fgetwc(in)) != WEOF)
        {
            c = Utils::filter_char(c);
            if (c == L'\0')
                continue;
            if (c == L' ')
            {
                word += (END_TOKEN + L" ");
                vocab[word] += 1;
                word = L"";
            }
            else
            {
                word += (c);
                word += L" ";
            }
        }

        fclose(in);

        return vocab;
    }

    vector<wstring> get_tokens()
    {
        unordered_map<wstring, int> tokens;

        for (auto word_count : vocab)
        {
            vector<wstring> tokens_list = Utils::split(word_count.first);
            for (wstring token : tokens_list)
            {
                tokens[token] += 1;
            }
        }

        vector<wstring> token_list;
        for (auto x : tokens)
            token_list.push_back(x.first);

        std::sort(token_list.begin(), token_list.end(), Utils::compare_len);

        return token_list;
    }

    unordered_map<wstring, int> get_pair_stats()
    {
        unordered_map<wstring, int> pair_and_count;
        for (auto word_count : vocab)
        {
            vector<wstring> symbols = Utils::split(word_count.first);
            for (int i = 0; i < symbols.size() - 1; i++)
            {
                pair_and_count[symbols[i] + L" " + symbols[i + 1]] += word_count.second;
            }
        }
        return pair_and_count;
    }

    wstring get_best_pair(unordered_map<wstring, int> pairs_and_counts)
    {
        int max_count = -1;
        wstring best_pair = L"";

        for (auto pair_count : pairs_and_counts)
        {
            if (pair_count.second > max_count)
            {
                max_count = pair_count.second;
                best_pair = pair_count.first;
            }
        }

        return best_pair;
    }

    unordered_map<wstring, int> merge_vocab(wstring best_pair, unordered_map<wstring, int> vocab_in)
    {
        unordered_map<wstring, int> vocab_out;

        wstring replacement = Utils::replace_all(best_pair, (L" "), L"");

        for (auto word_count : vocab_in)
        {
            wstring word_in = word_count.first;
            wstring word_out = word_in;
            word_out = Utils::replace_all(word_in, (best_pair), replacement);

            vocab_out[word_out] = word_count.second;
        }
        return vocab_out;
    }
};

int main()
{
    string model_path = "tokens.txt";
    TokenizerBPE tokenizer;
    // TokenizerBPE tokenizer("train-data.txt",1000);
    //tokenizer.fit();
    tokenizer.load_model(&model_path[0]);
    //tokenizer.save_model(&model_path[0]);
    // cout<<"Model Saved"<<endl;

    // wcout<<int(tokenizer.tokens[tokenizer.tokens.size()-1][0])<<endl;
    for (wstring a : tokenizer.encode(L"Salam, yetka, necəsən?", false))
        wcout << "[" << a << "] ";

    return 0;
}
