-- ==============================================================================
-- tbl_worksheet -> tbl_fileInfo 外键约束
-- ==============================================================================
-- 说明：
--   1. 本文件不参与 ODB 代码生成，需在 worksheetEntity.sql 执行完成后手动执行；
--   2. ODB 重新生成 worksheetEntity.sql 时不会覆盖本文件；
--   3. 重新执行 worksheetEntity.sql（会 DROP TABLE）后，需要重新执行本文件。
-- ==============================================================================

ALTER TABLE `tbl_worksheet`
  ADD CONSTRAINT `tbl_worksheet_fileId_fk`
  FOREIGN KEY (`fileId`)
  REFERENCES `tbl_fileInfo` (`fileId`)
  ON DELETE CASCADE;
