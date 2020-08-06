<?php
header("Content-Type: text/html;charset=utf-8");
include_once 'mysql.func.php';
session_start();
$link=new MySql();
$table='user';
$_SESSION['root'] = substr(dirname(__FILE__),0, -3);
