#include <iostream>
#include <sqlite3.h>

using namespace std;

int main()
{
    sqlite3* db;

    int result = sqlite3_open("members.db", &db);

    if (result == SQLITE_OK)
    {
        cout << "Database opened successfully" << endl;
    }
    else
    {
        cout << "Failed to open database" << endl;
    }

    sqlite3_close(db);

    return 0;
}