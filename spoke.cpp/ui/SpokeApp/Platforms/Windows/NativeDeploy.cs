// Windows packaging stub: ensure spoke.dll is deployed next to the app.
// In the .csproj, add (Windows target):
//   <Content Include="..\..\..\build\Release\spoke.dll">
//     <CopyToOutputDirectory>PreserveNewest</CopyToOutputDirectory>
//   </Content>
// Native lookup order: app dir → System32 → PATH. No code needed here;
// SpokeNativeService DllImport("spoke") resolves it automatically.
namespace SpokeApp.Platforms.Windows;

public static class NativeDeploy
{
    public const string LibraryName = "spoke.dll";
}
