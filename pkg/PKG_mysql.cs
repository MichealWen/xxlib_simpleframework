﻿using System;
using xx;

namespace PKG
{
    public static class MySqlAppendExt
    {
        public static void MySqlAppend<T>(this System.Text.StringBuilder sb, T v, bool ignoreReadOnly = false)
        {
            sb.Append(v);
        }

        public static void MySqlAppend<T>(this System.Text.StringBuilder sb, T? o, bool ignoreReadOnly = false) where T : struct
        {
            if (o.HasValue)
            {
                sb.MySqlAppend(o.Value);
            }
            else
            {
                sb.Append("null");
            }
        }

        public static void MySqlAppend(this System.Text.StringBuilder sb, string v, bool ignoreReadOnly = false)
        {
            sb.Append("'" + v.Replace("'", "''") + "'");
        }

        public static void MySqlAppend(this System.Text.StringBuilder sb, DateTime o, bool ignoreReadOnly = false)
        {
            sb.Append("'" + o.ToString("yyyy-MM-dd HH:mm:ss") + "'");
        }

        public static void MySqlAppend<T>(this System.Text.StringBuilder sb, List<T> os, bool ignoreReadOnly = false) { if (os == null || os.dataLen == 0) throw new ArgumentNullException(); for (int i = 0; i < os.dataLen; ++i) { sb.MySqlAppend(os[i], ignoreReadOnly); sb.Append(", "); }; sb.Length -= 2; }
        public static void MySqlAppend(this System.Text.StringBuilder sb, List<DateTime> os, bool ignoreReadOnly = false) { if (os == null || os.dataLen == 0) throw new ArgumentNullException(); sb.Append("( "); for (int i = 0; i < os.dataLen; ++i) { sb.MySqlAppend(os[i]); sb.Append(", "); }; sb.Length -= 2; sb.Append(" )"); }
        public static void MySqlAppend(this System.Text.StringBuilder sb, List<byte> os, bool ignoreReadOnly = false) { if (os == null || os.dataLen == 0) throw new ArgumentNullException(); sb.Append("( "); for (int i = 0; i < os.dataLen; ++i) { sb.Append(os[i]); sb.Append(", "); }; sb.Length -= 2; sb.Append(" )"); }
        public static void MySqlAppend(this System.Text.StringBuilder sb, List<ushort> os, bool ignoreReadOnly = false) { if (os == null || os.dataLen == 0) throw new ArgumentNullException(); sb.Append("( "); for (int i = 0; i < os.dataLen; ++i) { sb.Append(os[i]); sb.Append(", "); }; sb.Length -= 2; sb.Append(" )"); }
        public static void MySqlAppend(this System.Text.StringBuilder sb, List<uint> os, bool ignoreReadOnly = false) { if (os == null || os.dataLen == 0) throw new ArgumentNullException(); sb.Append("( "); for (int i = 0; i < os.dataLen; ++i) { sb.Append(os[i]); sb.Append(", "); }; sb.Length -= 2; sb.Append(" )"); }
        public static void MySqlAppend(this System.Text.StringBuilder sb, List<ulong> os, bool ignoreReadOnly = false) { if (os == null || os.dataLen == 0) throw new ArgumentNullException(); sb.Append("( "); for (int i = 0; i < os.dataLen; ++i) { sb.Append(os[i]); sb.Append(", "); }; sb.Length -= 2; sb.Append(" )"); }
        public static void MySqlAppend(this System.Text.StringBuilder sb, List<sbyte> os, bool ignoreReadOnly = false) { if (os == null || os.dataLen == 0) throw new ArgumentNullException(); sb.Append("( "); for (int i = 0; i < os.dataLen; ++i) { sb.Append(os[i]); sb.Append(", "); }; sb.Length -= 2; sb.Append(" )"); }
        public static void MySqlAppend(this System.Text.StringBuilder sb, List<short> os, bool ignoreReadOnly = false) { if (os == null || os.dataLen == 0) throw new ArgumentNullException(); sb.Append("( "); for (int i = 0; i < os.dataLen; ++i) { sb.Append(os[i]); sb.Append(", "); }; sb.Length -= 2; sb.Append(" )"); }
        public static void MySqlAppend(this System.Text.StringBuilder sb, List<int> os, bool ignoreReadOnly = false) { if (os == null || os.dataLen == 0) throw new ArgumentNullException(); sb.Append("( "); for (int i = 0; i < os.dataLen; ++i) { sb.Append(os[i]); sb.Append(", "); }; sb.Length -= 2; sb.Append(" )"); }
        public static void MySqlAppend(this System.Text.StringBuilder sb, List<long> os, bool ignoreReadOnly = false) { if (os == null || os.dataLen == 0) throw new ArgumentNullException(); sb.Append("( "); for (int i = 0; i < os.dataLen; ++i) { sb.Append(os[i]); sb.Append(", "); }; sb.Length -= 2; sb.Append(" )"); }
        public static void MySqlAppend(this System.Text.StringBuilder sb, List<double> os, bool ignoreReadOnly = false) { if (os == null || os.dataLen == 0) throw new ArgumentNullException(); sb.Append("( "); for (int i = 0; i < os.dataLen; ++i) { sb.Append(os[i]); sb.Append(", "); }; sb.Length -= 2; sb.Append(" )"); }
        public static void MySqlAppend(this System.Text.StringBuilder sb, List<float> os, bool ignoreReadOnly = false) { if (os == null || os.dataLen == 0) throw new ArgumentNullException(); sb.Append("( "); for (int i = 0; i < os.dataLen; ++i) { sb.Append(os[i]); sb.Append(", "); }; sb.Length -= 2; sb.Append(" )"); }
        public static void MySqlAppend(this System.Text.StringBuilder sb, List<bool> os, bool ignoreReadOnly = false) { if (os == null || os.dataLen == 0) throw new ArgumentNullException(); sb.Append("( "); for (int i = 0; i < os.dataLen; ++i) { sb.Append(os[i]); sb.Append(", "); }; sb.Length -= 2; sb.Append(" )"); }

        public static void MySqlAppend<T>(this System.Text.StringBuilder sb, Foo o, bool ignoreReadOnly = false)
        {
            sb.Append("(");
            sb.Append(o.id);
            sb.Append(", ");
            sb.MySqlAppend(o.age);
            sb.Append(", ");
            sb.MySqlAppend(o.info);
            sb.Append(", ");
            sb.Length -= 2;
            sb.Append(")");
        }
        public static void MySqlAppend<T>(this System.Text.StringBuilder sb, DataSet o, bool ignoreReadOnly = false)
        {
            sb.Append("(");
            sb.Length -= 2;
            sb.Append(")");
        }
        public static void MySqlAppend<T>(this System.Text.StringBuilder sb, Table o, bool ignoreReadOnly = false)
        {
            sb.Append("(");
            sb.Length -= 2;
            sb.Append(")");
        }
        public static void MySqlAppend<T>(this System.Text.StringBuilder sb, TableColumn o, bool ignoreReadOnly = false)
        {
            sb.Append("(");
            sb.Length -= 2;
            sb.Append(")");
        }
        public static void MySqlAppend<T>(this System.Text.StringBuilder sb, TableRow o, bool ignoreReadOnly = false)
        {
            sb.Append("(");
            sb.Length -= 2;
            sb.Append(")");
        }
        public static void MySqlAppend<T>(this System.Text.StringBuilder sb, TableRowValue o, bool ignoreReadOnly = false)
        {
            sb.Append("(");
            sb.Length -= 2;
            sb.Append(")");
        }
        public static void MySqlAppend<T>(this System.Text.StringBuilder sb, TableRowValue_Int o, bool ignoreReadOnly = false)
        {
            sb.Append("(");
            sb.Length -= 2;
            sb.Append(")");
        }
        public static void MySqlAppend<T>(this System.Text.StringBuilder sb, TableRowValue_NullableInt o, bool ignoreReadOnly = false)
        {
            sb.Append("(");
            sb.Length -= 2;
            sb.Append(")");
        }
        public static void MySqlAppend<T>(this System.Text.StringBuilder sb, TableRowValue_String o, bool ignoreReadOnly = false)
        {
            sb.Append("(");
            sb.Length -= 2;
            sb.Append(")");
        }
    }

    public class MySqlFuncs
    {
        public MySql.Data.MySqlClient.MySqlConnection conn;
        public MySql.Data.MySqlClient.MySqlCommand cmd;
        public System.Text.StringBuilder sb = new System.Text.StringBuilder();
        private List<int> recordsAffecteds = new List<int>();

        public List<int> RecordsAffecteds
        {
            get { return recordsAffecteds; }
        }

        public int RecordsAffected
        {
            get { return recordsAffecteds[0]; }
        }

        public MySqlFuncs(MySql.Data.MySqlClient.MySqlConnection conn)
        {
            this.conn = conn;
            this.cmd = conn.CreateCommand();
        }


        /// <summary>
        /// 模拟从表查数据填充到 Foo. 顺序和数量一致应该与 Foo 中标有 Column 的类成员一致
        /// </summary>
        public T SelectFoo<T>
        (
            int id,
            int? age,
            string info
            , MySql.Data.MySqlClient.MySqlTransaction tran_ = null) where T : Foo, new()
        {
            cmd.Transaction = tran_;
            recordsAffecteds.Clear();
            sb.Clear();
            sb.Append(@"select ");
            sb.Append(id);
            sb.Append(@", ");
            sb.MySqlAppend(age);
            sb.Append(@", ");
            sb.MySqlAppend(info);
            cmd.CommandText = sb.ToString();

            using (var r = cmd.ExecuteReader())
            {
                recordsAffecteds.Add(r.RecordsAffected);
                if (!r.Read())
                {
                    return null;
                }
                return new T
                {
                    id = r.GetInt32(0), 
                    age = r.IsDBNull(1) ? null : (int?)r.GetInt32(1), 
                    info = r.IsDBNull(2) ? null : (string)r.GetString(2)
                };
            }
        }
    }
}
