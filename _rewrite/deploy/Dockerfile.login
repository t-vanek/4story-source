# Multi-stage build: SDK image compiles, runtime image hosts. ~120 MB final.
FROM mcr.microsoft.com/dotnet/sdk:10.0 AS build
WORKDIR /src

# Copy the *.csproj files first so package restore caches well.
COPY FourStory.slnx ./
COPY Directory.Build.props ./
COPY FourStory.Shared/FourStory.Shared.csproj         FourStory.Shared/
COPY FourStory.Protocol/FourStory.Protocol.csproj     FourStory.Protocol/
COPY FourStory.Persistence/FourStory.Persistence.csproj FourStory.Persistence/
COPY FourStory.Login/FourStory.Login.csproj           FourStory.Login/
COPY FourStory.Map/FourStory.Map.csproj               FourStory.Map/
COPY FourStory.World/FourStory.World.csproj           FourStory.World/
COPY FourStory.Protocol.Tests/FourStory.Protocol.Tests.csproj FourStory.Protocol.Tests/
COPY FourStory.Login.IntegrationTests/FourStory.Login.IntegrationTests.csproj FourStory.Login.IntegrationTests/
RUN dotnet restore FourStory.Login/FourStory.Login.csproj

# Copy the rest and publish.
COPY . .
RUN dotnet publish FourStory.Login/FourStory.Login.csproj -c Release -o /app --no-restore

FROM mcr.microsoft.com/dotnet/runtime:10.0 AS runtime
WORKDIR /app
COPY --from=build /app .
ENV ASPNETCORE_URLS=
EXPOSE 4815
ENTRYPOINT ["dotnet", "FourStory.Login.dll"]
