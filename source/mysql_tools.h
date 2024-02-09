#pragma once
#include <mysql.h>
#include <vector>
#include "code_trans.h"
#include "fmt/format.h"
#include "fmt/core.h"

namespace details
{
	inline void sync_out(FILE* file, std::string_view s, bool new_line = false)
	{
		if (new_line)
		{
			const std::string ss{ std::string(s) + '\n' };
			fwrite(ss.data(), sizeof(char), ss.size(), file);  // NOLINT(cert-err33-c)
		}
		else
		{
			fwrite(s.data(), sizeof(char), s.size(), file);  // NOLINT(cert-err33-c)
		}
	}
	inline void sync_out(std::string_view s, bool new_line = false)
	{
		sync_out(stdout, s, new_line);
	}
}
template <typename... T>
FMT_INLINE void println(fmt::format_string<T...> fmt, T&&... args) {
	details::sync_out(fmt::vformat(fmt, fmt::make_format_args(args...)), true);
}
template <typename... T>
FMT_INLINE void print(fmt::format_string<T...> fmt, T&&... args) {
	details::sync_out(fmt::vformat(fmt, fmt::make_format_args(args...)), false);
}
namespace sql
{
	struct Setting
	{
		std::string host;
		std::string user; std::string passwd;
		std::string db;
		uint32_t port;
	};
	struct table
	{
		std::vector<std::string> field_names;
		std::vector<std::vector<std::string>> values;
	};

	inline void print_errno(MYSQL& mysql);
	inline void print_info(MYSQL& mysql);

	inline std::shared_ptr<MYSQL> create_mysql(const Setting& setting);
	inline int real_query(MYSQL& mysql, std::string_view q);
	inline std::shared_ptr<MYSQL_RES> store_result(MYSQL& mysql);
	inline auto real_query_and_store(MYSQL& mysql, std::string_view q);

	inline std::vector<std::string> fetch_field_names(MYSQL_RES* res);
	inline std::vector<std::vector<std::string>> fetch_table_values(MYSQL_RES* res);
	inline table fetch_table(MYSQL_RES* res);

	inline void println(MYSQL_RES* res);
	inline void println(const std::shared_ptr<MYSQL_RES>& res);


	inline std::string_view get_view(char* p)
	{
		return p ? std::string_view(p) : std::string_view{};
	}



	inline void print_errno(MYSQL& mysql)
	{
		::println("error: {}", mysql_error(&mysql));
	}

	inline void print_info(MYSQL& mysql)
	{
		::println(mysql_info(&mysql));
	}

	inline std::shared_ptr<MYSQL> create_mysql(const Setting& setting)
	{
		std::shared_ptr<MYSQL> mysql(new MYSQL, [](MYSQL* p)
			{
				::mysql_close(p);
				delete p;
			});
		mysql_init(mysql.get());
		if (!mysql_real_connect(mysql.get(), setting.host.c_str()
			, setting.user.c_str(), setting.passwd.c_str(), setting.db.c_str(), setting.port, nullptr, 0))
		{
			return std::shared_ptr<MYSQL>{};
		}
		return mysql;
	}


	inline int real_query(MYSQL& mysql, std::string_view q)
	{
		return mysql_real_query(&mysql, q.data(), q.size());
	}

	inline std::shared_ptr<MYSQL_RES> store_result(MYSQL& mysql)
	{
		return std::shared_ptr<MYSQL_RES>{
			mysql_store_result(&mysql), [](auto& p)
			{
				::mysql_free_result(p);
			}
		};
	}
	inline auto real_query_and_store(MYSQL& mysql, std::string_view q)
	{

		if (real_query(mysql, q))
		{
			sql::print_errno(mysql);
			return std::shared_ptr<MYSQL_RES>{};
		}
		return store_result(mysql);
	}


	inline std::vector<std::string> fetch_field_names(MYSQL_RES* res)
	{
		std::vector<std::string> col_names;
		for(unsigned i = 0; i < res->field_count; ++i)
		{
			col_names.emplace_back(res->fields[i].name);
		}
		return col_names;
	}
	inline std::vector<std::vector<std::string>> fetch_table_values(MYSQL_RES* res)
	{
		std::vector<std::vector<std::string>> table;

		const MYSQL_ROWS* row = res->data->data;
		for (unsigned r = 0; r < res->data->rows; ++r)
		{
			std::vector<std::string> table_row;
			for (unsigned i = 0; i < res->data->fields; ++i)
				table_row.emplace_back(get_view(row->data[i]));
			row = row->next;
			table.emplace_back(std::move(table_row));
		}
		return table;
	}

	inline table fetch_table(MYSQL_RES* res)
	{
		return{
			fetch_field_names(res),
			fetch_table_values(res)
		};
	}


	inline void println(MYSQL_RES* res)
	{
		auto print_row_line = [res]()
		{
			for (unsigned i = 0; i < res->field_count; ++i)
			{
				auto field = res->fields[i];
				print("+{:-<{}}", '-', std::max<unsigned long>(res->fields[i].name_length, res->fields[i].max_length));
			}
			::println("+");
		};
		auto col_width = [res](unsigned i)
		{
			return std::max<unsigned long>(res->fields[i].name_length, res->fields[i].max_length);
		};

		print_row_line();
		for (unsigned i = 0; i < res->field_count; ++i)
		{
			auto field = res->fields[i];
			print("|{:<{}}", Utf8ToAnsi(res->fields[i].name), col_width(i));
		}
		::println("|");

		print_row_line();

		const MYSQL_ROWS* rows = res->data->data;
		for (unsigned r = 0; r < res->data->rows; ++r)
		{
			for (unsigned i = 0; i < res->data->fields; ++i)
				print("|{:<{}}", Utf8ToAnsi(get_view(rows->data[i])), col_width(i));
			::println("|");
			rows = rows->next;

		}
		print_row_line();
	}

	inline void println(const std::shared_ptr<MYSQL_RES>& res)
	{
		println(res.get());
	}
};