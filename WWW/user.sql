CREATE TABLE `user` (
  
`id` int(11) NOT NULL AUTO_INCREMENT,
  
`username` varchar(30) NOT NULL,
  
`passwd` varchar(255) NOT NULL,
  
`nickname` varchar(50) DEFAULT 'нч',
  
`email` varchar(50) DEFAULT 'нч',
  
`signature` varchar(255) DEFAULT 'нч',  
`imgindex` int not null default 0,
  
PRIMARY KEY (`id`)
) ENGINE=MyISAM AUTO_INCREMENT=17 DEFAULT CHARSET=utf8