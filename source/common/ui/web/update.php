<?php

//
// Aspia Project
// Copyright (C) 2016-2022 Dmitry Chapyshev <dmitry@aspia.ru>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

require_once 'config.php';

$mysqli = new mysqli(Config::$db_host, Config::$db_user, Config::$db_pass, Config::$db_name);
$mysqli->set_charset('utf8');

if (mysqli_connect_errno())
{
	die('Could not connect! ' . $mysqli->connect_error);
}

parse_str($_SERVER["QUERY_STRING"], $query);

$package = $mysqli->real_escape_string($query['package']);
$version = $mysqli->real_escape_string($query['version']);

if (!empty($package) and !empty($version))
{
	if (!$result = $mysqli->query("SELECT id FROM packages WHERE name='$package'"))
	{
		die('Failed to execute database query! ' . $mysqli->error);
	}

	if ($result->num_rows != 0)
	{
		$package = $result->fetch_array()['id'];
		$version = implode('.', array_slice(explode('.', $version), 0, 3));

		if (!$result = $mysqli->query("SELECT target_version, description, url FROM updates WHERE package_id = '$package' AND source_version = '$version'"))
		{
			die('Failed to execute database query! ' . $mysqli->error);
		}

		$xml = new SimpleXMLElement('<?xml version="1.0" encoding="UTF-8"?><update/>');

		if ($result->num_rows != 0)
		{
			$update = $result->fetch_array();

			$xml->addChild('version', $update['target_version']);
			$xml->addChild('description', $update['description']);
			$xml->addChild('url', $update['url']);
		}

		header('Content-Type: application/xml');
		print($xml->asXML());
	}
	else
	{
		die('Package not found!');
	}
}
else
{
	die('Invalid request received!');
}

$mysqli->close();

?>