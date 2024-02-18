#ifndef LESSON1_FILE_H
#define LESSON1_FILE_H

#include <string>
#include <mutex>

extern std::mutex m;
class File {
public:
    static void copyFile(const std::string& path1, const std::string& path2, const int socket);
    static void getInfo(const std::string& path1, const int socket);
};


#endif
