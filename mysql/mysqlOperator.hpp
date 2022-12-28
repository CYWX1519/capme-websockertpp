#include <mutex>
#include <mysql/mysql.h>
#include <plog/Log.h>

namespace mysqlOperator
{
// define the mysql param
// mysql connect HOST
#define MYSQL_USING_HOST "localhost"
// username
#define MYSQL_USER "rane"
// user password
#define MYSQL_PASSWD ""
// mysql connect port
#define MYSQL_PORT 3306
// using database
#define MYSQL_USEING_DATABASE ""
// instance id
#define MYSQL_CURRENT_PID 0

    class mysqlOperator
    {
    private:
        MYSQL mysql;
        std::mutex connectionListMutex;

    public:
        mysqlOperator();
        ~mysqlOperator();
    };
} // namespace mysqlOperator
