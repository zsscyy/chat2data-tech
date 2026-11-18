-- ==============================================================================
-- 将 tbl_worksheet 的列名从 dbTableName 重命名为 tableName
-- ==============================================================================
-- 说明：
--   1. 本脚本用于已有数据库，保留数据，不 DROP 表；
--   2. 全新数据库直接使用重新生成的 worksheetEntity.sql 即可；
--   3. 脚本通过 information_schema 判断旧列是否存在，可重复执行。
-- ==============================================================================

SET @has_old_column := (
  SELECT COUNT(*)
  FROM information_schema.columns
  WHERE table_schema = DATABASE()
    AND table_name = 'tbl_worksheet'
    AND column_name = 'dbTableName'
);

SET @rename_sql := IF(
  @has_old_column > 0,
  'ALTER TABLE `tbl_worksheet` CHANGE COLUMN `dbTableName` `tableName` VARCHAR(64) CHARACTER SET utf8mb4 NOT NULL',
  'SELECT ''tbl_worksheet.tableName already exists, skip rename'''
);

PREPARE rename_stmt FROM @rename_sql;
EXECUTE rename_stmt;
DEALLOCATE PREPARE rename_stmt;
