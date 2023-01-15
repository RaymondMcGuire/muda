#pragma once
#include "base.h"

namespace muda
{
/// <summary>
/// a viwer that allows to access a CSR sparse matrix
/// </summary>
/// <typeparam name="T"></typeparam>
template <typename T>
class csr
{
  public:
    class elem
    {
        int     row_;
        int     col_;
        int     global_offset_;
        csr<T>& csr_;
        MUDA_GENERIC elem(csr<T>& csr, int row, int col, int global_offset) noexcept
            : csr_(csr)
            , row_(row)
            , col_(col)
            , global_offset_(global_offset)
        {
        }

      public:
        friend class csr<T>;
        //trivial copy constructor
        MUDA_GENERIC elem(const elem& e) = default;
        //trivial copy assignment
        MUDA_GENERIC elem& operator=(const elem& e) = default;
        MUDA_GENERIC       operator const T&() const noexcept
        {
            return csr_.m_values[global_offset_];
        }
        MUDA_GENERIC operator T&() noexcept
        {
            return csr_.m_values[global_offset_];
        }
        Eigen::Vector<int, 2> pos() const noexcept
        {
            return Eigen::Vector<int, 2>(row_, col_);
        }
        int global_offset() const noexcept { return global_offset_; }

        MUDA_GENERIC T& operator=(const T& v) noexcept
        {
            auto& pos = csr_.m_values[global_offset_];
            pos       = v;
            return pos;
        }
    };
    class celem
    {
        int           row_;
        int           col_;
        int           global_offset_;
        const csr<T>& csr_;
        MUDA_GENERIC celem(const csr<T>& csr, int row, int col, int global_offset) noexcept
            : csr_(csr)
            , row_(row)
            , col_(col)
            , global_offset_(global_offset)
        {
        }

      public:
        friend class csr<T>;
        //trivial copy constructor
        MUDA_GENERIC celem(const celem& e) = default;
        //trivial copy assignment
        MUDA_GENERIC celem& operator=(const celem& e) = default;
        MUDA_GENERIC        operator const T&() const noexcept
        {
            return csr_.m_values[global_offset_];
        }
        Eigen::Vector<int, 2> pos() const noexcept
        {
            return Eigen::Vector<int, 2>(row_, col_);
        }
        int global_offset() const noexcept { return global_offset_; }
    };

    MUDA_GENERIC csr() noexcept
        : m_values(nullptr)
        , m_colIdx(nullptr)
        , m_rowPtr(nullptr)
        , m_nnz(0)
        , m_rows(0)
        , m_cols(0)
    {
    }
    
    MUDA_GENERIC csr(int* rowPtr, int* colIdx, T* values, int rows, int cols, int nNonZeros) noexcept
        : m_rowPtr(rowPtr)
        , m_colIdx(colIdx)
        , m_values(values)
        , m_nnz(nNonZeros)
        , m_rows(rows)
        , m_cols(cols)
    {
    }
    
    // rows getter
    MUDA_GENERIC int rows() const noexcept { return m_rows; }
    // cols getter
    MUDA_GENERIC int cols() const noexcept { return m_cols; }
    // nnz getter
    MUDA_GENERIC int nnz() const noexcept { return m_nnz; }

    // get by row and col as if it is a dense matrix
    MUDA_GENERIC T operator()(int row, int col) const noexcept
    {
        check_range(row, col);
        for(int i = m_rowPtr[row]; i < m_rowPtr[row + 1]; i++)
        {
            if(m_colIdx[i] == col)
                return m_values[i];
        }
        return 0;
    }
    // read-write element
    MUDA_GENERIC elem rw_elem(int row, int local_offset) noexcept
    {
        int global_offset;
        check_all(row, local_offset, global_offset);
        return elem(*this, row, m_colIdx[global_offset], global_offset);
    }
    // read-only element
    MUDA_GENERIC celem ro_elem(int row, int local_offset) const noexcept
    {
        int global_offset;
        check_all(row, local_offset, global_offset);
        return celem(*this, row, m_colIdx[global_offset], global_offset);
    }

    MUDA_GENERIC void place_row(int row, int global_offset) noexcept
    {
        check_row(row);
        m_rowPtr[row] = global_offset;
    }

    MUDA_GENERIC void place_tail() noexcept { m_rowPtr[m_rows] = m_nnz; }

    MUDA_GENERIC int place_col(int row, int local_offset, int col) noexcept
    {
        check_row(row);
        int global_offset = m_rowPtr[row] + local_offset;
        check_global_offset(global_offset);
        m_colIdx[global_offset] = col;
        return global_offset;
    }

    MUDA_GENERIC int place_col(int row, int local_offset, int col, const T& v) noexcept
    {
        check_row(row);
        int global_offset = m_rowPtr[row] + local_offset;
        check_global_offset(global_offset);
        m_colIdx[global_offset] = col;
        m_values[global_offset] = v;
        return global_offset;
    }

    MUDA_GENERIC int place_col(int global_offset, int col, const T& v) noexcept
    {
        check_global_offset(global_offset);
        m_colIdx[global_offset] = col;
        m_values[global_offset] = v;
        return global_offset;
    }

    MUDA_GENERIC int nnz(int row) const noexcept
    {
        check_row(row);
        return m_rowPtr[row + 1] - m_rowPtr[row];
    }

  private:
    int* m_rowPtr;
    int* m_colIdx;
    T*   m_values;
    int  m_nnz;
    int  m_rows;
    int  m_cols;
    MUDA_GENERIC __forceinline__ void check_range(int row, int col) const noexcept
    {
        if constexpr(DEBUG_VIEWER)
            if(row < 0 || row >= m_rows || col < 0 || col >= m_cols)
            {
                muda_kernel_error("csr: row/col index out of range: index=(%d,%d) dim_=(%d,%d)\n",
                                   row,
                                   col,
                                   m_rows,
                                   m_cols);
            }
    }

    MUDA_GENERIC __forceinline__ void check_row(int row) const noexcept
    {
        if constexpr(DEBUG_VIEWER)
            if(row < 0 || row >= m_rows)
            {
                muda_kernel_error("csr: row index out of range: index=(%d) rows=(%d)\n", row, m_rows);
            }
    }

    MUDA_GENERIC __forceinline__ void check_local_offset(int row, int offset) const noexcept
    {
        if constexpr(DEBUG_VIEWER)
            if(row < 0 || row >= m_rows || offset < 0
               || offset >= m_rowPtr[row + 1] - m_rowPtr[row])
            {
                muda_kernel_error(
                    "csr: 'rowPtr[row] + offset > rowPtr[row+1]' out of range:\n"
                    "row=%d, offset=%d, rowPtr[row]=%d, rowPtr[row+1]=%d\n",
                    row,
                    offset,
                    m_rowPtr[row],
                    m_rowPtr[row + 1]);
            }
    }

    MUDA_GENERIC __forceinline__ void check_global_offset(int globalOffset) const noexcept
    {
        if constexpr(DEBUG_VIEWER)
            if(globalOffset < 0 || globalOffset >= m_nnz)
            {
                muda_kernel_error("csr: globalOffset out of range: globalOffset=%d, nnz=%d\n",
                                   globalOffset,
                                   m_nnz);
            }
    }

    MUDA_GENERIC __forceinline__ void check_all(int row, int local_offset, int& global_offset) const noexcept
    {
        check_row(row);
        check_local_offset(row, local_offset);
        global_offset = m_rowPtr[row] + local_offset;
        check_global_offset(global_offset);
    }
};
}  // namespace muda
