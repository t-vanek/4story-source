#pragma once

// EntityMapping<T> — table-to-struct mapping traits.
//
// Specialize this template for each entity type to enable Repository<T>.
// The specialization must provide:
//
//   static constexpr const char* Table;        -- table name
//   static constexpr const char* PkColumn;     -- primary-key column name
//   static T          FromRow(const soci::row&);
//   static std::string SelectAllSql();         -- "SELECT ... FROM Table"
//   static std::string SelectByIdSql();        -- + "WHERE PK = :pk"
//   static std::string InsertSql();            -- INSERT with named params
//   static std::string UpdateSql();            -- UPDATE with named params
//   static std::string DeleteSql();            -- DELETE WHERE pk = :pk
//   static void  BindInsert(soci::statement&, const T&);
//   static void  BindUpdate(soci::statement&, const T&);
//   static auto  GetPk(const T&);             -- returns PK value
//
// Example for a simple Account entity:
//
//   template<>
//   struct fourstory::db::orm::EntityMapping<Account> {
//       static constexpr const char* Table    = "TACCOUNT_PW";
//       static constexpr const char* PkColumn = "dwUserID";
//
//       static Account FromRow(const soci::row& r) {
//           Account a;
//           a.dwUserID = r.get<int>(0);
//           a.szUserID = r.get<std::string>(1);
//           return a;
//       }
//       static std::string SelectAllSql() {
//           return "SELECT dwUserID, szUserID FROM TACCOUNT_PW";
//       }
//       static std::string SelectByIdSql() {
//           return SelectAllSql() + " WHERE dwUserID = :pk";
//       }
//       static std::string InsertSql() {
//           return "INSERT INTO TACCOUNT_PW (szUserID, szPasswd) "
//                  "VALUES (:uid, :pwd)";
//       }
//       static std::string UpdateSql() {
//           return "UPDATE TACCOUNT_PW SET szPasswd = :pwd "
//                  "WHERE dwUserID = :pk";
//       }
//       static std::string DeleteSql() {
//           return "DELETE FROM TACCOUNT_PW WHERE dwUserID = :pk";
//       }
//       static void BindInsert(soci::statement& st, const Account& a) {
//           st , soci::use(a.szUserID, "uid")
//              , soci::use(a.szPasswd, "pwd");
//       }
//       static void BindUpdate(soci::statement& st, const Account& a) {
//           st , soci::use(a.szPasswd, "pwd")
//              , soci::use((int)a.dwUserID, "pk");
//       }
//       static int GetPk(const Account& a) { return a.dwUserID; }
//   };

namespace soci { class session; class row; class statement; }

namespace fourstory::db::orm {

// Primary template — unspecialized: gives a clear compile error
// rather than a confusing linker failure.
template<typename T>
struct EntityMapping
{
    static_assert(sizeof(T) == 0,
        "fourstory::db::orm::EntityMapping<T> must be specialized "
        "for this entity type. See entity_mapping.h for instructions.");
};

} // namespace fourstory::db::orm
