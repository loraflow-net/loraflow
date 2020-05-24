/* 
Please run this file in MariDB (MySQL) connsole using following steatment: \. /path/to/file/db_install.sql
 */
/**
 * Author:  loraflow
 * Created: May 11, 2020
 */


/* In order to install MariaDB (free version of MySQL)
1) run sudo apt-get install mariadb-server
2) sudo mysql_secure_installation
3) sudo mysql -u root -p
*/

CREATE DATABASE IF NOT EXISTS loraflow;
USE loraflow;


/* Create default lora user */

CREATE USER 'lora'@'localhost' IDENTIFIED BY '<put_your_password_here>';
GRANT ALL PRIVILEGES ON loraflow.* TO 'lora'@'localhost';
FLUSH PRIVILEGES;


/* 
Sensors table. New fields will be added automatically based on their first value
String, float, decimal. Please adjust type if it is neccessary.
*/

CREATE TABLE `sensor_data` (
  `id` int(11) unsigned NOT NULL AUTO_INCREMENT,
  `device` varchar(254) NOT NULL DEFAULT '',
  `received` datetime NOT NULL,
  PRIMARY KEY (`id`),
  KEY `device` (`device`)
) ENGINE=InnoDB AUTO_INCREMENT=0 DEFAULT CHARSET=utf8;


CREATE TABLE `raw_msg_received` (
  `id` int(11) unsigned NOT NULL AUTO_INCREMENT,
  `received` datetime DEFAULT NULL,
  `msg` text DEFAULT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=5478 DEFAULT CHARSET=utf8;