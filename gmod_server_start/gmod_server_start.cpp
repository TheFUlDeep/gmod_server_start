#include <iostream>
#include <fstream>
#include <string>
#include <stack>
using namespace std;

typedef stack<string> argsstack;

argsstack FileLinesToStack(const char *file)
{
    argsstack stck;
    std::ifstream f(file);
    if (f.is_open()) 
    {
        std::string line;
        while (std::getline(f, line)) stck.push(line);
        f.close();
    }
    return stck;
}

const char* argsminus[]{ "insecure","authkey" };
const char* argsplus[]{ "sv_setsteamaccount","host_workshop_collection","maxplayers","map","gamemode" };

const string GetStartArgs()
{
    string str = "";

    auto linesstack = FileLinesToStack("server\\startargs.txt");

    while (!linesstack.empty())
    {
        string line = linesstack.top();
        linesstack.pop();

        //пропускаю закомментированные строки или неправильные строки (в которых нет знака =) или FALSE
        if (line[0] == '#' || line.find("=FALSE") != string::npos) continue;
        size_t equalpos = line.find('=');
        if (equalpos == string::npos) continue;

        for (string word : argsminus)
        {
            if (line.find(word) == string::npos) continue;
            string leftpart = '-' + word;
            string rightpart;
            if (line.find("=TRUE") != string::npos) rightpart = "";
            else rightpart = ' ' + line.substr(equalpos + 1);
            str += (leftpart + rightpart + ' ');
        }

        for (string word : argsplus)
        {
            if (line.find(word) == string::npos) continue;
            string leftpart = '+' + word;
            string rightpart;
            if (line.find("=TRUE") != string::npos) rightpart = "";
            else rightpart = ' ' + line.substr(equalpos + 1);
            str += (leftpart + rightpart + ' ');
        }

    }

    return str;
}

int main(int argc, char** argv)
{
    string name = argv[0];
    auto backslash = name.rfind("\\");
    if (backslash != string::npos) name = name.substr(backslash + 1);

    setlocale(LC_ALL, "english");

    auto adminargsstack = FileLinesToStack("adminargs.txt");
    const string affinity = adminargsstack.top(); adminargsstack.pop();
    const string port = adminargsstack.top(); adminargsstack.pop();

    string windowtitle1 = name + " server watchdog";
    const string start = ("@echo off\ncls\necho Protecting srcds from crashes...\ntitle " + windowtitle1 + "\n:srcds\necho (%date%) (%time%) srcds started.\n");
    const string prestartargs = (start + "cd ./server\nstart /wait /high /min /affinity " + affinity + " srcds.exe -nocrashdialog -console -tickrate 20 -autoupdate -game garrysmod -port " + port + " ");
    const string end = "\necho (%date%) (%time%) WARNING: srcds closed or crashed, restarting.\ngoto srcds";

    const string startargs = GetStartArgs();

    const string batcommand = (prestartargs + startargs + end);

    
    ofstream f("start.bat");
    if (f.is_open())
    {
        f << batcommand;
        f.close();
        //cout << "added start" << endl;
    }

    string windowtitle2 = name + " start_of_start";
    f = ofstream("start_of_start.bat");
    if (f.is_open())
    {
        f << string("@echo off\ncls\necho Protecting " + windowtitle1 +" from crashes...\ntitle " + windowtitle2 + "\n:srcds\necho (%date%) (%time%) " + windowtitle1 + " started.\nstart /wait start.bat\necho (%date%) (%time%) WARNING : " + windowtitle1 + " closed or crashed, restarting.\ngoto srcds");
        f.close();
        //cout << "added start_of_start" << endl;
    }

    //system("pause");
    system("start start_of_start.bat");
    //-condebug
    return 0;
}