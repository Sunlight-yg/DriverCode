<?php
include_once 'config.php';
class MySql{
    public static $link=null;

    function __construct(){
        $configs=array(
            'dsn'=>DB_TYPE.':host:'.DB_USER.';dbname='.DB_NAME,
            'username'=>DB_USER,
            'pwd'=>DB_PWD
            );
        $link=new PDO($configs['dsn'],$configs['username'],$configs['pwd']);
        $link->exec('USE test');
        self::$link=$link;
    }

    function insert($table,$array){
        $keys=join(",",array_keys($array));
        $vals="'".join("','",array_values($array))."'";
        $sql="INSERT INTO ".$table."({$keys}) VALUES({$vals})";
        $res=self::$link->exec($sql);
        if($res)
            return true;
        return false;
    }

    function update($table,$array,$username){
    	$sets = null;
        foreach($array as $key=>$val){
            $sets.=$key.'="'.$val.'",';
        }
        $sets=rtrim($sets,',');
        $sql="UPDATE ".$table." SET ".$sets." WHERE username=".$username;
        $res=self::$link->exec($sql);
        if($res)
            return true;
        return false;
    }

    function select($table,$username,$passwd){
        $username=addslashes($username);
        $sql="SELECT username FROM ".$table." WHERE username='{$username}' AND passwd='{$passwd}'";
        $stmt=self::$link->query($sql);
        if($stmt->rowCount()) 
                return true;
        return false;
    }

    function getRow($table,$username){
        $sql="SELECT * FROM ".$table." WHERE username='{$username}'";
        $stmt=self::$link->query($sql);
        return $stmt->fetch(PDO::FETCH_ASSOC);
    }
}
//跳转
function skip($mes,$url){
    echo "<script>alert('{$mes}');
        window.location.href='{$url}';
        </script>";
}
//遍历目录
function read_all ($dir, &$arr=null){
    if(!is_dir($dir)) return false;
    $handle = opendir($dir);
    if($handle){
        while(($name = readdir($handle)) !== false){
            if($name!='.' && $name != '..')
            {
            	//echo '文件：'.$name."<br>";
				if(!is_null($arr))            	
					$arr[] = $name;
            }
        }
    }
}