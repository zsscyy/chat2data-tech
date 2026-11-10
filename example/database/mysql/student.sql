-- 如果testdb数据库不存在则创建
CREATE DATABASE IF NOT EXISTS testdb DEFAULT CHARSET utf8mb4;

-- 切换到 testdb数据库
USE testdb;

-- 创建数据库表student，包含字段：学生id、学生姓名、学生性别、学生成绩
CREATE TABLE IF NOT EXISTS student (
    id BIGINT UNSIGNED NOT NULL PRIMARY KEY,
    name VARCHAR(100) NOT NULL,
    age TINYINT UNSIGNED NOT NULL,
    gender CHAR(1) NOT NULL,
    score DECIMAL(5,2) NOT NULL
);