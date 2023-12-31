Function readBinary(strPath)

    Dim oFSO: Set oFSO = CreateObject("Scripting.FileSystemObject")
    Dim oFile: Set oFile = oFSO.GetFile(strPath)

    If IsNull(oFile) Then MsgBox("File not found: " & strPath) : Exit Function

    With oFile.OpenAsTextStream()
        readBinary = .Read(oFile.Size)
        .Close
    End With

End Function

Function writeBinary(strBinary, strPath)

    Dim oFSO: Set oFSO = CreateObject("Scripting.FileSystemObject")

    ' below lines pupose: checks that write access is possible!
    Dim oTxtStream

    On Error Resume Next
    Set oTxtStream = oFSO.createTextFile(strPath)

    If Err.number <> 0 Then MsgBox(Err.message) : Exit Function
    On Error GoTo 0

    Set oTxtStream = Nothing
    ' end check of write access

    With oFSO.createTextFile(strPath)
        .Write(strBinary)
        .Close
    End With

End Function
