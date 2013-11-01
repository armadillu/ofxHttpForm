<html>
<body>

<?php

	if ($_FILES["myFile"]["error"] > 0){
		echo "Return Code: " . $_FILES["myFile"]["error"] . "\n";
	}else{

		$tempFileName = $_FILES["myFile"]["tmp_name"];
		$fileName = $_FILES["myFile"]["name"];
		$path = "uploaded/";
		$type = $_FILES["myFile"]["type"];
		$size = ($_FILES["myFile"]["size"] / 1024); //kb
		$completePath = $path.date("H:i:s")."_".$fileName;
		
		echo "Uploaded: " . $fileName . "\n";
		echo "Type: " . $type . "\n";
		echo "Size: " . $size . " Kb". "\n";
		echo "Temp ul file: " . $tempFileName . "\n";			

		move_uploaded_file($tempFileName, $completePath);
		echo "Stored in: " . $completePath . "\n";
	}
?>

</body>
</html>