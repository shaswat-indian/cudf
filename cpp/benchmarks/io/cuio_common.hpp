/*
 * Copyright (c) 2020-2023, NVIDIA CORPORATION.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <cudf_test/file_utilities.hpp>

#include <cudf/io/data_sink.hpp>
#include <cudf/io/datasource.hpp>
#include <cudf/io/types.hpp>

#include <rmm/device_uvector.hpp>

using cudf::io::io_type;

#define WR_BENCHMARK_DEFINE_ALL_SINKS(benchmark, name, type_or_group)                          \
  benchmark(name##_file_output, type_or_group, static_cast<uint32_t>(io_type::FILEPATH));      \
  benchmark(name##_buffer_output, type_or_group, static_cast<uint32_t>(io_type::HOST_BUFFER)); \
  benchmark(name##_void_output, type_or_group, static_cast<uint32_t>(io_type::VOID));

std::string random_file_in_dir(std::string const& dir_path);

/**
 * @brief Class to create a coupled `source_info` and `sink_info` of given type.
 */
class cuio_source_sink_pair {
  class bytes_written_only_sink : public cudf::io::data_sink {
    size_t _bytes_written = 0;

   public:
    void host_write(void const* data, size_t size) override { _bytes_written += size; }
    void flush() override {}
    size_t bytes_written() override { return _bytes_written; }
  };

 public:
  cuio_source_sink_pair(io_type type);
  ~cuio_source_sink_pair()
  {
    // delete the temporary file
    std::remove(file_name.c_str());
  }
  /**
   * @brief Created a source info of the set type
   *
   * The `datasource` created using the returned `source_info` will read data from the same location
   * that the result of a @ref `make_sink_info` call writes to.
   *
   * @return The description of the data source
   */
  cudf::io::source_info make_source_info();

  /**
   * @brief Created a sink info of the set type
   *
   * The `data_sink` created using the returned `sink_info` will write data to the same location
   * that the result of a @ref `make_source_info` call reads from.
   *
   * `io_type::DEVICE_BUFFER` source/sink is an exception where a host buffer sink will be created.
   *
   * @return The description of the data sink
   */
  cudf::io::sink_info make_sink_info();

  [[nodiscard]] size_t size();

 private:
  static temp_directory const tmpdir;

  io_type const type;
  std::vector<char> h_buffer;
  rmm::device_uvector<std::byte> d_buffer;
  std::string const file_name;
  bytes_written_only_sink void_sink;
};

/**
 * @brief Column selection strategy.
 */
enum class column_selection { ALL, ALTERNATE, FIRST_HALF, SECOND_HALF };

/**
 * @brief Row selection strategy.
 *
 * Not all strategies are applicable to all readers.
 */
enum class row_selection { ALL, BYTE_RANGE, NROWS, SKIPFOOTER, STRIPES, ROW_GROUPS };

/**
 * @brief Modify data types such that total selected columns size is a fix fraction of the total
 * size.
 *
 * The data types are multiplied/rearranged such that the columns selected with the given column
 * selection enumerator add up to a fixed fraction of the total table size, regardless of the data
 * types.
 *
 * @param ids Array of column type IDs
 * @param cs The column selection enumerator
 *
 * @return The duplicated/rearranged array of type IDs
 */
std::vector<cudf::type_id> dtypes_for_column_selection(std::vector<cudf::type_id> const& ids,
                                                       column_selection col_sel);

/**
 * @brief Selects a subset of columns based on the input enumerator.
 */
std::vector<int> select_column_indexes(int num_cols, column_selection col_sel);

/**
 * @brief Selects a subset of columns from the array of names, based on the input enumerator.
 */
std::vector<std::string> select_column_names(std::vector<std::string> const& col_names,
                                             column_selection col_sel);

/**
 * @brief Returns file segments that belong to the given chunk if the file is split into a given
 * number of chunks.
 *
 * The segments could be Parquet row groups or ORC stripes.
 */
std::vector<cudf::size_type> segments_in_chunk(int num_segments, int num_chunks, int chunk);

/**
 * @brief Drops L3 cache if `CUDF_BENCHMARK_DROP_CACHE` environment variable is set.
 *
 * Has no effect if the environment variable is not set.
 * May require sudo access ro run successfully.
 *
 * @throw cudf::logic_error if the environment variable is set and the command fails
 */
void try_drop_l3_cache();
