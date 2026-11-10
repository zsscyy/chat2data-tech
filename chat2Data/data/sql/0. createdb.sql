-- 创建 chat2Data 数据库（字符集必须为 utf8mb4）
CREATE DATABASE IF NOT EXISTS chat2Data DEFAULT CHARSET utf8mb4;

-- 切换到 chat2Data 数据库
USE chat2Data;

-- 建表顺序：先执行本文件，再执行同目录下的 userEntity.sql、sessionEntity.sql
-- 注意：ODB 生成的建表 SQL 带 DROP TABLE，重复执行会清空数据
