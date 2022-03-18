#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <istream>
#include <fstream>
#include <iterator>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <array>
#include <regex>

using namespace std;


const string WHITESPACE = " \n\r\t\f\v\'\"";
//trimming left side
string ltrim(const string& str)
{
    size_t start = str.find_first_not_of(WHITESPACE);
    return (start == string::npos) ? "" : str.substr(start);
}
//trimming right side
string rtrim(const string& str)
{
    size_t end = str.find_last_not_of(WHITESPACE);
    return (end == string::npos) ? "" : str.substr(0, end + 1);
}
//main trim call
string trim(string str)
{
    return rtrim(ltrim(str));
}

// specific version of trim for the awk function:
string trim_for_awk(string str)
{
    if (str.find("\'") == -1) {
        return str;
    }
    size_t nxt = str.find_last_of("\'");
    str = str.substr(1, nxt - 1);
    while (str.find(" ") != -1)
    {
        size_t space = str.find_last_of(" ");
        str = str.substr(0, space) + str.substr(space + 1);
    }

    return str;
}
//split function by space
vector<string> split(string line, string separator = " ")
{
    line = trim(line);
    vector<string> str;
    size_t start_pos;
    size_t end_pos = 0;
    while ((start_pos = line.find_first_not_of(separator, end_pos)) != string::npos)
    {
        end_pos = line.find(separator, start_pos);
        str.push_back(line.substr(start_pos, end_pos - start_pos));
    }
    return str;
}

//used to be able to call execvp() later on
char **vec_to_char_array(vector<string> parts)
{
    char **arr = new char *[parts.size() + 1];
    for (int i = 0; i < parts.size(); i++)
    {
        arr[i] = new char[parts[i].size()];
        strcpy(arr[i], parts[i].c_str());
    }
    arr[parts.size()] = NULL;
    return arr;
}
//used to get runtime for user prompt
char* gettime()
{
    time_t rawtime;
    time (&rawtime);
    return ctime (&rawtime);
}
//getting user id for shell user prompt
string get_user_id(){
    string user_id;
    user_id = system("whoami");
    user_id = trim(user_id);
    return user_id;
}


int main (){
    dup2(0,10);
    vector<int> zombie;
    ////////////////
    char buff[1000]; //buffer size
	string curr_dir = getcwd(buff,sizeof(buff));
    chdir(".."); //change the directory to the previous path
    string prev_dir = getcwd(buff,sizeof(buff)); // assign previous directory tp prev_dir
    chdir(curr_dir.c_str()); //change the directory back to the current directory

    while (true){
        dup2(10,0);
        bool bgp = false;

        for(int i = 0; i < zombie.size(); i++) {
            if (waitpid(zombie[i], 0, WNOHANG) == zombie[i]) {
                zombie.erase(zombie.begin() + i);
                i--;
            }

        }
        
        //prompt
        cout << "DATE: " << trim(gettime()) << " " << endl;
        printf(getenv("USER"));
        cout << ":-My Shell$ " ; //printing user prompt
        
        string inputline;
        getline (cin, inputline); //get a line from standard input

        if (inputline == string("exit")){
            cout << "Bye!! End of shell" << endl;
            break;
        }

        //echo
        if (trim(inputline).find("echo") == 0) {
            inputline = inputline.erase(0,5);
            inputline = trim(inputline);
            cout << inputline << endl;
            continue; 
        }
            

        //background processes
        int pos = inputline.find('&');
        if(pos != -1){
            inputline = trim(inputline.substr(0,pos-1));
            bgp = true;
        }

        //piping
        vector<string> pipeParts = split(inputline, "|");
        for (int i = 0; i < pipeParts.size(); i++) {
            inputline = trim(pipeParts.at(i));
            int fds[2];
            pipe(fds);
           
            //directory proceessing
            if(trim(inputline).find("cd") ==0){
                curr_dir = getcwd(buff,sizeof(buff));
                if (trim(inputline).find("-") == 3) {
                    chdir(prev_dir.c_str());
                    prev_dir = curr_dir;
                }
                else{
                    string dirname = trim(split(inputline)[1]);
                    chdir(dirname.c_str());
                    prev_dir = curr_dir;
                }
                continue;
            }

            int pid = fork ();
            if (pid == 0){ //child process

                //check awk
                if (inputline.find("awk") == 0) {
                    inputline = "awk " + trim_for_awk(inputline.substr(inputline.find("\'")));
                }

                //I/O redirection
                int pos = inputline.find('>');
                if (pos >= 0)
                {
                    string cmmnd = trim(inputline.substr(0, pos));
                    string filename = trim(inputline.substr(pos + 1));

                    inputline = cmmnd;

                    int fd = open(filename.c_str(), O_WRONLY|O_CREAT, S_IWUSR|S_IRUSR);
                    dup2(fd, 1);
                    close(fd);
                }

                int pos_1 = inputline.find('<');
                if (pos_1 >= 0)
                {
                    string cmmnd = trim(inputline.substr(0, pos_1));
                    string filename = trim(inputline.substr(pos_1 + 1));

                    inputline = cmmnd;

                    int fd = open(filename.c_str(), O_RDONLY|O_CREAT, S_IWUSR|S_IRUSR);
                    dup2(fd, 0);
                    close(fd);

                }
        
                if (i < pipeParts.size() - 1) {
                    dup2(fds[1], 1);
                    close(fds[1]);
                }

                vector<string> parts = split(inputline);

                char** args = vec_to_char_array(parts);
                execvp (args [0], args);
            } else{
                //getting rid of zombies
                if (!bgp)
                {
                    if (i == pipeParts.size() - 1){
                        waitpid(pid, 0, 0);
                    } else{
                        //push pid into zombie process
                        zombie.push_back(pid);
                    }
                } else{
                    //push pid into zombie process
                    zombie.push_back(pid);
                }

                dup2(fds[0], 0);
                close(fds[1]);
            }
        }
    }
}
        
